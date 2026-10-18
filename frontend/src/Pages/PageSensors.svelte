<script lang="ts">
  import {
    Status,
    type DeviceStatus,
    type SensorStatus,
    type StatusData,
  } from "../interfaces";
  import settingsIcon from "../assets/settings.svg";
  import Icon from "../lib/Icon.svelte";
  import WaitBox from "../lib/WaitBox.svelte";
  import Indicator from "../lib/Indicator.svelte";

  export let data: StatusData | null = null;
  export let onConfigure: (thermostat: DeviceStatus) => void = () => {};
  let sensors: SensorStatus[] | null = null;
  let thermostats: DeviceStatus[] = [];
  let timestamp: number = 0;

  $: updateSensorsAndTimestamp(data);
  $: fritzStatus = data?.fritz?.status;
  $: fritzConnecting =
    fritzStatus === Status.configured || fritzStatus === Status.connecting;
  $: unboundSensors = (sensors ?? []).filter(
    (sensor) => !thermostats.some((v) => v.referenceSensor == sensor.id)
  );

  // Function to update sensors data
  function updateSensorsAndTimestamp(data: StatusData | null) {
    if (data && data.sensors) {
      thermostats = data.thermostats ?? [];
      sensors = data.sensors ?? [];
      if (data.sensors.length > 0) {
        data.sensors.sort((a, b) => a.id.localeCompare(b.id));
        timestamp = Math.max(...data.sensors.map((v) => v.seriesTimestamp));
      } else {
        timestamp = 0;
      }
    } else {
      sensors = null;
    }
  }
</script>

{#if sensors === null}
  <WaitBox
    message="trying to connect to the gateway"
    details="this buddy gossips too much with the sensors"
  />
{:else if sensors.length === 0 && thermostats.length === 0 && fritzStatus === Status.connected}
  <WaitBox
    message="trying hard to find sensors nearby"
    details="FritzGate uses the Theengs Decoder to support over 70 models of temperature sensors."
  />
{:else}
  <article>
    <header>
      <strong>Thermostats</strong>
      <a href="#setup" class="secondary">
        <Icon src={settingsIcon} label="Change FRITZ!Box credentials" />
      </a>
      {#if thermostats.length > 0}
        <small>hold a gauge to configure it</small>
      {/if}
    </header>
    {#if fritzStatus === Status.unconfigured || fritzStatus === Status.failure}
      <div class="connect">
        <p>
          {fritzStatus === Status.failure
            ? "FritzGate couldn't log in to the FRITZ!Box with the saved credentials."
            : "FritzGate isn't connected to a FRITZ!Box yet."}
        </p>
        <a href="#setup" role="button">Connect to FRITZ!Box</a>
      </div>
    {:else if fritzConnecting}
      <p class="empty" aria-busy="true">connecting to FRITZ!Box</p>
    {:else if thermostats.length === 0}
      <p class="empty">no thermostats found on the FRITZ!Box yet</p>
    {/if}
    <div class="gauges">
      <!-- Every thermostat, showing its reference sensor or, without one, its built-in reading -->
      {#each thermostats as thermostat (thermostat.id)}
        {@const sensor = sensors.find(
          (v) => v.id == thermostat.referenceSensor
        )}
        <div>
          <Indicator
            title={thermostat.name}
            temperature={sensor?.temperature ?? thermostat.temperature}
            humidity={sensor ? Math.round(sensor.humidity) : undefined}
            titleLeft={(thermostat.model ?? "") + " " + (thermostat.serial ?? "")}
            signalLeft={thermostat.rssi}
            chargeLeft={thermostat.battery}
            targetTemperature={thermostat.targetTemperature}
            temperatureLeft={sensor ? thermostat.temperature : undefined}
            titleRight={sensor?.name ??
              (thermostat.referenceSensor
                ? `${thermostat.referenceSensor} (unavailable)`
                : "built-in sensor")}
            signalRight={sensor?.rssi}
            chargeRight={sensor?.battery}
            showRightBadges={!!thermostat.referenceSensor}
            onHold={() => onConfigure(thermostat)}
          />
        </div>
      {/each}
    </div>
  </article>

  {#if unboundSensors.length > 0}
    <article>
      <header>
        <strong>Sensors</strong>
        <small>not used as reference by any thermostat</small>
      </header>
      <div class="gauges">
        {#each unboundSensors as sensor (sensor.id)}
          <div>
            <Indicator
              title="Unknown location"
              temperature={sensor.temperature}
              humidity={Math.round(sensor.humidity)}
              titleLeft="no thermostat"
              titleRight={sensor.name}
              signalRight={sensor.rssi}
              chargeRight={sensor.battery}
            />
          </div>
        {/each}
      </div>
    </article>
  {/if}
{/if}

<style>
  header {
    display: flex;
    align-items: baseline;
    flex-wrap: wrap;
    gap: 0 0.5em;
  }

  header small,
  .empty {
    color: var(--pico-muted-color);
  }

  .connect {
    text-align: center;
  }

  /* As many gauges per row as fit at 11.5rem or more, but always two on phones;
     the gap keeps the curved labels of neighbouring gauges apart */
  .gauges {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(min(11.5rem, 45%), 1fr));
    gap: 0.25rem;
  }

  @media (min-width: 576px) {
    .gauges {
      gap: 1rem;
    }
  }
</style>
