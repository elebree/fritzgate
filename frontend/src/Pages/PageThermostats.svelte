<script lang="ts">
  import {
    Status,
    type ApiService,
    type DeviceStatus,
    type SensorStatus,
    type StatusData,
  } from "../interfaces";
  import DevicesList from "../lib/DevicesList.svelte";
  import WaitBox from "../lib/WaitBox.svelte";

  export let api: ApiService;
  export let data: StatusData;

  let thermostats: DeviceStatus[] = [];
  let sensors: SensorStatus[] = [];
  var timestamp = 0;

  $: {
    if (data) {
      if (
        data?.fritz?.status == Status.failure ||
        data?.fritz?.status == Status.unconfigured ||
        data?.fritz?.status == Status.configured ||
        data?.fritz?.status == Status.connecting
      ) {
        location.hash = "#setup";
      }
      ({ thermostats, sensors } = data);
      timestamp =
        thermostats && thermostats.length > 0
          ? Math.max(...thermostats.map((v) => v.seriesTimestamp))
          : 0;
    }
  }
</script>

<p class="connection">
  <small>
    FRITZ!Box <strong>{data?.fritz?.host ?? ""}</strong>, user
    <strong>{data?.fritz?.user ?? ""}</strong> ·
    <a href="#setup">change credentials</a>
  </small>
</p>

{#if !thermostats || thermostats.length === 0}
  <WaitBox
    message="waiting for data from FRITZ!Box"
    details="can't find any thermostat yet"
  />
{:else}
  <DevicesList devices={thermostats} {sensors} {api} />
{/if}

<style>
  .connection {
    margin-bottom: var(--pico-spacing);
    color: var(--pico-muted-color);
  }
</style>
