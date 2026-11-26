<script lang="ts">
  import "./app.scss";

  import PageSensors from "./Pages/PageSensors.svelte";
  import DiagnosticsDialog from "./lib/DiagnosticsDialog.svelte";
  import FritzBoxDialog from "./lib/FritzBoxDialog.svelte";
  import Icon from "./lib/Icon.svelte";
  import SensorDialog from "./lib/SensorDialog.svelte";
  import ThermostatDialog from "./lib/ThermostatDialog.svelte";
  import WaitBox from "./lib/WaitBox.svelte";
  import type { ApiService, DeviceStatus, SensorStatus, StatusData } from "./interfaces";
  import github from "./assets/github-mark.svg";
  import diagnosticsIcon from "./assets/diagnostics.svg";

  // Dialogs open on top of the dashboard: #setup for the FRITZ!Box, #thermostat-<id> for a thermostat, #sensor-<id>
  // for a sensor that no thermostat uses, #diagnostics for the state and log of the board
  const THERMOSTAT_HASH = "#thermostat-";
  const SENSOR_HASH = "#sensor-";
  const DIAGNOSTICS_HASH = "#diagnostics";
  const STATUS_POLL_MS = 2000;

  export let api: ApiService;
  let state: StatusData | undefined = undefined;
  let page: string = location.hash;
  // True when opening the dialog added a history entry, so closing it goes back instead of piling up entries
  let dialogInHistory = false;
  let busy = false;
  let timer: ReturnType<typeof setTimeout>;
  // Only the latest status request schedules the next one
  let statusRequest = 0;

  $: configured = page.startsWith(THERMOSTAT_HASH)
    ? state?.thermostats.find(
        (v) => v.id === decodeURIComponent(page.slice(THERMOSTAT_HASH.length))
      )
    : undefined;
  $: shownSensor = page.startsWith(SENSOR_HASH)
    ? state?.sensors.find((v) => v.id === decodeURIComponent(page.slice(SENSOR_HASH.length)))
    : undefined;

  function isDialog(hash: string) {
    return (
      hash === "#setup" ||
      hash === DIAGNOSTICS_HASH ||
      hash.startsWith(THERMOSTAT_HASH) ||
      hash.startsWith(SENSOR_HASH)
    );
  }

  function setPage(hash: string) {
    const diagnosticsClosed = page === DIAGNOSTICS_HASH && hash !== DIAGNOSTICS_HASH;
    page = hash;
    // the dashboard waited while the diagnostics dialog was open: bring it up to date right away
    if (diagnosticsClosed) {
      fetchData();
    }
  }

  function routeChange() {
    console.log(location.hash);
    const previous = page;
    setPage(location.hash);
    dialogInHistory = isDialog(page) && !isDialog(previous);
  }

  function configure(thermostat: DeviceStatus) {
    location.hash = THERMOSTAT_HASH + encodeURIComponent(thermostat.id);
  }

  function showSensor(sensor: SensorStatus) {
    location.hash = SENSOR_HASH + encodeURIComponent(sensor.id);
  }

  function closeDialog() {
    if (!isDialog(page)) {
      return;
    }
    if (dialogInHistory) {
      history.back();
    } else {
      history.replaceState(null, "", location.pathname + location.search);
      setPage("");
    }
  }

  // The diagnostics dialog asks the board for its state every few seconds itself, and the board answers one request at
  // a time: while the dialog is open, the dashboard under it isn't refreshed. setPage() starts again when it closes.
  async function fetchData() {
    clearTimeout(timer);
    if (page === DIAGNOSTICS_HASH) {
      return;
    }
    const current = ++statusRequest;
    try {
      const newData: StatusData | undefined = await api.fetchStatus();
      if (current !== statusRequest) {
        return;
      }
      if (!newData) {
        busy = true;
        return;
      }

      state = {
        version: newData.version,
        fritz: { ...newData.fritz },
        sensors: newData.sensors.map((v) => ({
          ...v,
          rssi: v.rssi ? v.rssi : undefined!,
        })),
        thermostats: newData.thermostats.map((v) => ({ ...v })),
      };

      busy = false;
    } catch {
      // request failed
      if (current === statusRequest) {
        busy = true;
      }
    } finally {
      if (current === statusRequest && page !== DIAGNOSTICS_HASH) {
        timer = setTimeout(fetchData, STATUS_POLL_MS);
      }
    }
  }
  fetchData();
</script>

<svelte:window on:hashchange={routeChange} />
<main>
  <nav>
    <ul>
      <li>
        <strong>FritzGate</strong>
        {#if state?.version}<small class="version">{state.version}</small>{/if}
        <span aria-busy={busy}></span>
      </li>
    </ul>
    <ul>
      <li>
        <a class="secondary" href={DIAGNOSTICS_HASH}
          ><Icon src={diagnosticsIcon} label="Diagnostics" --icon-size="1.5em" /></a
        >
      </li>
      <li>
        <a
          class="secondary"
          href="https://github.com/elebree/fritzgate"
          target="_blank"
          ><Icon src={github} label="Github" --icon-size="1.5em" /></a
        >
      </li>
    </ul>
  </nav>
  {#if !state}
    <WaitBox
      message="trying to connect to the FritzGate"
      details="this buddy gossips too much with sensors"
    />
  {:else}<PageSensors data={state} onConfigure={configure} onShowSensor={showSensor} />
  {/if}
  {#if state && page === "#setup"}
    <FritzBoxDialog {api} data={state} onClose={closeDialog} />
  {:else if page === DIAGNOSTICS_HASH}
    <DiagnosticsDialog {api} onClose={closeDialog} />
  {:else if state && configured}
    <ThermostatDialog
      thermostat={configured}
      thermostats={state.thermostats}
      sensors={state.sensors}
      {api}
      onClose={closeDialog}
    />
  {:else if state && shownSensor}
    <SensorDialog sensor={shownSensor} {api} onClose={closeDialog} />
  {/if}
</main>

<style>
  .version {
    color: var(--pico-muted-color);
  }
</style>
