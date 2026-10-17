<script lang="ts">
  import heatIcon from "../assets/heating.svg";
  import type { DeviceStatus } from "../interfaces";
  import BatteryIcon from "./BatteryIcon.svelte";
  import SignalIcon from "./SignalIcon.svelte";
  import HumidityIcon from "./HumidityIcon.svelte";
  import Icon from "./Icon.svelte";
  import TemperatureIcon from "./TemperatureIcon.svelte";

  export let device: Partial<DeviceStatus>;
</script>

<!-- Fixed columns keep the thermostat and sensor rows of a card aligned -->
<div class="indicators">
  <span
    class:strikeout={device.referenceSensor}
    title={device.referenceSensor
      ? `Measured ${device.temperature}℃, replaced by the reference sensor`
      : `Actual temperature ${device.temperature}℃`}
  >
    <TemperatureIcon value={device.temperature} />
    {device.temperature?.toLocaleString(undefined, {
      minimumFractionDigits: 1,
      maximumFractionDigits: 1,
    })}℃
  </span>
  {#if device.targetTemperature}
    <span title="Target temperature {device.targetTemperature}℃">
      <Icon src={heatIcon} />
      {device.targetTemperature}℃
    </span>
  {:else if device.humidity}
    <span title="Actual humidity {device.humidity}%">
      <HumidityIcon value={device.humidity} />
      {Math.round(device.humidity)}%
    </span>
  {:else}
    <span></span>
  {/if}
  <SignalIcon value={device.rssi} />
  <BatteryIcon value={device.battery} />
</div>

<style>
  .indicators {
    display: grid;
    grid-template-columns: 5.5em 5em 1.5em 1.5em;
    gap: 0.75em;
    align-items: center;
    white-space: nowrap;
    font-size: 0.875em;
  }
</style>
