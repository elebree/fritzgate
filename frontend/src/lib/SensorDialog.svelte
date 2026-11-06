<script lang="ts">
  import type { ApiService, SensorStatus } from "../interfaces";
  import Dialog from "./Dialog.svelte";
  import HistoryPanel from "./HistoryPanel.svelte";
  import Indicators from "./Indicators.svelte";

  export let sensor: SensorStatus;
  export let api: ApiService;
  export let onClose: () => void;
</script>

<!-- A sensor that no thermostat uses, with the history of its readings -->
<Dialog {onClose}>
  <svelte:fragment slot="header">
    <div class="title">
      <strong>{sensor.name}</strong>
      <small>{sensor.id}</small>
    </div>
    <Indicators device={sensor} />
  </svelte:fragment>

  <HistoryPanel {api} {sensor} />
</Dialog>

<style>
  .title {
    margin-bottom: 0.75em;
  }

  .title strong,
  .title small {
    display: block;
  }

  small {
    color: var(--pico-muted-color);
  }
</style>
