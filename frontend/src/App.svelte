<script lang="ts">
  import "./app.scss";

  import PageSensors from "./Pages/PageSensors.svelte";
  import FritzBoxDialog from "./lib/FritzBoxDialog.svelte";
  import Icon from "./lib/Icon.svelte";
  import ThermostatDialog from "./lib/ThermostatDialog.svelte";
  import WaitBox from "./lib/WaitBox.svelte";
  import type { ApiService, DeviceStatus, StatusData } from "./interfaces";
  import github from "./assets/github-mark.svg";

  // Dialogs open on top of the dashboard: #setup for the FRITZ!Box, #thermostat-<id> for a thermostat
  const THERMOSTAT_HASH = "#thermostat-";

  export let api: ApiService;
  let state: StatusData | undefined = undefined;
  let page: string = location.hash;
  // True when opening the dialog added a history entry, so closing it goes back instead of piling up entries
  let dialogInHistory = false;
  let busy = false;
  let timer: ReturnType<typeof setTimeout>;

  $: configured = page.startsWith(THERMOSTAT_HASH)
    ? state?.thermostats.find(
        (v) => v.id === decodeURIComponent(page.slice(THERMOSTAT_HASH.length))
      )
    : undefined;

  function isDialog(hash: string) {
    return hash === "#setup" || hash.startsWith(THERMOSTAT_HASH);
  }

  function routeChange() {
    console.log(location.hash);
    const previous = page;
    page = location.hash;
    dialogInHistory = isDialog(page) && !isDialog(previous);
  }

  function configure(thermostat: DeviceStatus) {
    location.hash = THERMOSTAT_HASH + encodeURIComponent(thermostat.id);
  }

  function closeDialog() {
    if (!isDialog(page)) {
      return;
    }
    if (dialogInHistory) {
      history.back();
    } else {
      history.replaceState(null, "", location.pathname + location.search);
      page = "";
    }
  }

  function requestNotifications() {
    Notification.requestPermission(function (result) {
      if (result === "granted") {
        // new Notification("Notifications permission granted")
      }
    });
  }

  async function fetchData() {
    try {
      const newData: StatusData | undefined = await api.fetchStatus();
      if (!newData) {
        busy = true;
        return;
      }

      if (state) {
        var newSensors = newData.sensors.filter(
          (v) => !state?.sensors.find((ns) => ns.name == v.name)
        );
        if (newSensors.length > 0) {
          new Notification(
            "Found a new sensor(s): " + newSensors.map((v) => v.name).join(", ")
          );
        }
      }

      state = {
        fritz: { ...newData.fritz },
        sensors: newData.sensors.map((v) => ({
          ...v,
          rssi: v.rssi ? v.rssi : undefined!,
        })),
        thermostats: newData.thermostats.map((v) => ({ ...v })),
      };

      busy = false;
    } catch {
      busy = true;
      // request failed
    } finally {
      timer = setTimeout(fetchData, 2000);
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
        <span aria-busy={busy}></span>
      </li>
    </ul>
    <ul>
      <!-- <li><button on:click={requestNotifications}>Notifications</button></li> -->
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
  {:else}<PageSensors data={state} onConfigure={configure} />
  {/if}
  {#if state && page === "#setup"}
    <FritzBoxDialog {api} data={state} onClose={closeDialog} />
  {:else if state && configured}
    <ThermostatDialog
      thermostat={configured}
      thermostats={state.thermostats}
      sensors={state.sensors}
      {api}
      onClose={closeDialog}
    />
  {/if}
</main>
