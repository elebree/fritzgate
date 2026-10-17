<script lang="ts">
  import type { DeviceStatus, SensorStatus, StatusData } from "../interfaces";
  import WaitBox from "../lib/WaitBox.svelte";
  import Indicator from "../lib/Indicator.svelte";

  export let data: StatusData | null = null;
  let sensors: SensorStatus[] | null = null;
  let thermostats: DeviceStatus[] = [];
  let timestamp: number = 0;

  $: updateSensorsAndTimestamp(data);

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
{:else if sensors.length === 0}
  <WaitBox
    message="trying hard to find sensors nearby"
    details="FritzGate uses the Theengs Decoder to support over 70 models of temperature sensors."
  />
{:else}
  <article class="devices-list">
    <div class="row gauges">
      {#each sensors as sensor}
        {@const thermostat = thermostats.find(
          (v) => v.referenceSensor == sensor.id
        )}
        <div class="col-6 col-xs-6 col-sm-6 col-md-4 col-lg-3 col-xl-3">
          <Indicator
            title={thermostat?.name ?? "Unknown location"}
            temperature={sensor.temperature}
            humidity={Math.round(sensor.humidity)}
            titleLeft={thermostat ? (thermostat?.model ?? "") +
              " " +
              (thermostat?.serial ?? "") : "no thermostat"}
            signalLeft={thermostat?.rssi}
            chargeLeft={thermostat?.battery}
            targetTemperature={thermostat?.targetTemperature}
            temperatureLeft={thermostat?.temperature}
            titleRight={sensor.name}
            signalRight={sensor.rssi}
            chargeRight={sensor.battery}
          />
        </div>
      {/each}
    </div>
  </article>
{/if}

<style>
  /* Bootstrap gutters keep the curved labels of neighbouring gauges apart */
  .gauges {
    --bs-gutter-x: 0.25rem;
    --bs-gutter-y: 0.25rem;
  }

  @media (min-width: 576px) {
    .gauges {
      --bs-gutter-x: 1rem;
      --bs-gutter-y: 1rem;
    }
  }
</style>
