<script lang="ts">
  import type { ApiService, DeviceStatus, SensorStatus } from "../interfaces";
  import Indicators from "./Indicators.svelte";

  export let devices: DeviceStatus[];
  export let sensors: SensorStatus[] | undefined;
  export let api: ApiService;

  async function onReferenceSensorChange(
    device: DeviceStatus,
    event: Event & { currentTarget: EventTarget & HTMLSelectElement }
  ) {
    const response = await api.setBinding(device.id, event.currentTarget.value);
    if (!response.ok) {
      // Handle
    }
  }

  function formatTemperature(value: number | undefined) {
    return value?.toLocaleString(undefined, {
      minimumFractionDigits: 1,
      maximumFractionDigits: 1,
    });
  }
</script>

<div class="devices-list">
  {#each devices as device (device.id)}
    {@const reference = sensors?.find((sensor) => sensor.id === device.referenceSensor)}
    <article>
      <header>
        <div class="title">
          <div>
            <strong>{device.name}</strong>
            <small>{device.model} {device.serial}</small>
          </div>
          {#if device.offset != null}
            <small class="offset" title="Temperature offset set on the thermostat">
              offset {device.offset > 0 ? "+" : ""}{formatTemperature(device.offset)}℃
            </small>
          {/if}
        </div>
        <Indicators {device} />
      </header>

      {#if sensors}
        <label>
          <small>Reference sensor</small>
          <select
            bind:value={device.referenceSensor}
            on:change={(event) => onReferenceSensorChange(device, event)}
          >
            <option value="">
              Built-in thermostat sensor ({formatTemperature(device.temperature)}℃)
            </option>
            {#each sensors as sensor (sensor.id)}
              <option value={sensor.id}>
                {sensor.name} ({formatTemperature(sensor.temperature)}℃)
              </option>
            {/each}
            {#if device.referenceSensor && !reference}
              <option value={device.referenceSensor}>
                {device.referenceSensor} (unavailable)
              </option>
            {/if}
          </select>
        </label>
        {#if reference}
          <Indicators device={reference} />
        {/if}
      {/if}
    </article>
  {/each}
</div>

<style>
  .devices-list {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(min(100%, 18rem), 1fr));
    gap: var(--pico-spacing);
  }

  article {
    margin: 0;
  }

  .title {
    display: flex;
    justify-content: space-between;
    align-items: start;
    gap: var(--pico-spacing);
    margin-bottom: 0.75em;
  }

  .title strong,
  .title small {
    display: block;
  }

  small {
    color: var(--pico-muted-color);
  }

  .offset {
    white-space: nowrap;
  }

  label small {
    display: block;
    margin-bottom: 0.25em;
  }

  select {
    margin-bottom: 0.5em;
    font-size: 0.875em;
  }
</style>
