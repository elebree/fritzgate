<script lang="ts">
  import type { ApiService, DeviceStatus, SensorStatus } from "../interfaces";
  import Dialog from "./Dialog.svelte";
  import Indicators from "./Indicators.svelte";

  export let thermostat: DeviceStatus;
  export let thermostats: DeviceStatus[];
  export let sensors: SensorStatus[];
  export let api: ApiService;
  export let onClose: () => void;

  let dialog: Dialog;
  // Kept locally and sent only on Save, so the status refresh every 2 s doesn't reset the choice
  let selected = thermostat.referenceSensor ?? "";
  let saving = false;
  let failed = false;

  $: reference = sensors.find((sensor) => sensor.id === selected);
  $: changed = selected !== (thermostat.referenceSensor ?? "");
  // Names of the other thermostats per reference sensor, to show which sensors are already in use
  $: otherUsers = thermostats.reduce((users, other) => {
    if (other.id !== thermostat.id && other.referenceSensor) {
      users.set(other.referenceSensor, [
        ...(users.get(other.referenceSensor) ?? []),
        other.name,
      ]);
    }
    return users;
  }, new Map<string, string[]>());

  async function save() {
    saving = true;
    failed = false;
    try {
      const response = await api.setBinding(thermostat.id, selected);
      if (response.ok) {
        dialog.close();
        return;
      }
      failed = true;
    } catch {
      failed = true;
    } finally {
      saving = false;
    }
  }

  function usedBy(sensorId: string, users: Map<string, string[]>) {
    const names = users.get(sensorId);
    return names ? ` · used by ${names.join(", ")}` : "";
  }

  function formatTemperature(value: number | undefined) {
    return value?.toLocaleString(undefined, {
      minimumFractionDigits: 1,
      maximumFractionDigits: 1,
    });
  }
</script>

<Dialog bind:this={dialog} {onClose}>
  <svelte:fragment slot="header">
    <div class="title">
      <div>
        <strong>{thermostat.name}</strong>
        <small>{thermostat.model} {thermostat.serial}</small>
      </div>
      {#if thermostat.offset != null}
        <small class="offset" title="Temperature offset set on the thermostat">
          offset {thermostat.offset > 0 ? "+" : ""}{formatTemperature(
            thermostat.offset
          )}℃
        </small>
      {/if}
    </div>
    <Indicators device={thermostat} />
  </svelte:fragment>

  <label>
    <small>Reference sensor</small>
    <select bind:value={selected} disabled={saving}>
      <option value="">
        Built-in thermostat sensor ({formatTemperature(thermostat.temperature)}℃)
      </option>
      {#each sensors as sensor (sensor.id)}
        <option value={sensor.id}>
          {sensor.name} ({formatTemperature(sensor.temperature)}℃){usedBy(
            sensor.id,
            otherUsers
          )}
        </option>
      {/each}
      {#if thermostat.referenceSensor && !sensors.some((sensor) => sensor.id === thermostat.referenceSensor)}
        <option value={thermostat.referenceSensor}>
          {thermostat.referenceSensor} (unavailable)
        </option>
      {/if}
    </select>
  </label>
  {#if reference}
    <Indicators device={reference} />
  {/if}
  {#if otherUsers.has(selected)}
    <p class="note">
      <small>
        Also the reference sensor for {otherUsers.get(selected)?.join(", ")}.
      </small>
    </p>
  {/if}
  {#if failed}
    <p class="note">
      <mark>couldn't save the reference sensor, please try again</mark>
    </p>
  {/if}

  <svelte:fragment slot="footer">
    <button class="secondary" on:click={() => dialog.close()}>Cancel</button>
    <button on:click={save} disabled={!changed || saving} aria-busy={saving}>
      Save
    </button>
  </svelte:fragment>
</Dialog>

<style>
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
  }

  .note {
    margin: 0.5em 0 0;
  }
</style>
