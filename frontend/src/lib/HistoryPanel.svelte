<script lang="ts">
  import { onDestroy } from "svelte";
  import type { ApiService, DeviceStatus, HistoryData, SensorStatus } from "../interfaces";
  import { formatTime, type ChartSeries } from "./chart";
  import HistoryChart from "./HistoryChart.svelte";

  export let api: ApiService;
  /** The thermostat of the dialog, none for a sensor without thermostat */
  export let thermostat: DeviceStatus | undefined = undefined;
  /** The sensor of the dialog, or the reference sensor chosen in a thermostat dialog, if any */
  export let sensor: SensorStatus | undefined = undefined;

  const RANGES = [
    { label: "24 h", hours: 24, step: 5 * 60 },
    { label: "7 days", hours: 7 * 24, step: 30 * 60 },
    { label: "30 days", hours: 30 * 24, step: 2 * 60 * 60 },
  ];
  // FritzGate adds a sample every minute
  const REFRESH_MS = 60 * 1000;
  // FritzGate answers one request at a time, and none while it syncs with the FRITZ!Box: a failed request is tried again
  // after each of these delays before the panel reports it
  const RETRY_MS = [1000, 2000, 4000];

  // Colors follow the device: the sensor's readings in one, the thermostat's in another
  const SENSOR_COLOR = "var(--history-sensor)";
  const THERMOSTAT_COLOR = "var(--history-thermostat)";
  const TARGET_COLOR = "var(--history-target)";

  /** A value of the readout, the charts and the table */
  interface Column {
    key: string;
    label: string;
    color: string;
    dashed?: boolean;
    values: (number | null)[];
    format: (value: number) => string;
  }

  let range = RANGES[0];
  let data: HistoryData | undefined = undefined;
  // The hours of the loaded data, which stays shown while another range loads
  let dataHours = range.hours;
  let loading = false;
  let failed = false;
  let hoverIndex: number | undefined = undefined;
  let tableOpen = false;
  // Only the answer to the latest request counts, and none after the dialog closed
  let request = 0;
  let timer: ReturnType<typeof setTimeout> | undefined;
  // Retries made since the last answer, or since another range was chosen
  let retries = 0;

  async function load() {
    clearTimeout(timer);
    const current = ++request;
    const hours = range.hours;
    loading = true;
    let retryMs: number | undefined = undefined;
    try {
      const result = await api.fetchHistory(hours, range.step);
      if (current === request) {
        data = result;
        dataHours = hours;
        failed = false;
        retries = 0;
      }
    } catch {
      if (current === request) {
        if (retries < RETRY_MS.length) {
          retryMs = RETRY_MS[retries++];
        } else {
          failed = true;
          retries = 0;
        }
      }
    } finally {
      if (current === request) {
        // still loading while a retry is due
        loading = retryMs !== undefined;
        timer = setTimeout(load, retryMs ?? REFRESH_MS);
      }
    }
  }

  function selectRange(selected: (typeof RANGES)[number]) {
    if (selected !== range) {
      range = selected;
      hoverIndex = undefined;
      retries = 0;
      load();
    }
  }

  load();
  onDestroy(() => {
    request++;
    clearTimeout(timer);
  });

  // Made once, as toLocaleString() with options makes a new formatter for every value of the table
  const tenths = new Intl.NumberFormat(undefined, { minimumFractionDigits: 1, maximumFractionDigits: 1 });
  const celsius = (value: number) => tenths.format(value) + "℃";
  const signedCelsius = (value: number) => (value > 0 ? "+" : "") + celsius(value);
  const percent = (value: number) => Math.round(value) + "%";
  const hasValues = (values: (number | null)[]) => values.some((value) => value != null);

  /**
   * The arrays of /api/history end with the current step and leave out the steps before their first value: put them
   * back as gaps, so that every array has a value per point
   */
  function alignEnd(values: (number | null)[] | undefined, count: number): (number | null)[] {
    const kept = (values ?? []).slice(-count);
    return [...Array<null>(count - kept.length).fill(null), ...kept];
  }

  // The points of the loaded data or, until it is there, of the chosen range up to now
  $: stepSeconds = data?.step ?? range.step;
  $: hours = data ? dataHours : range.hours;
  $: count = Math.ceil((hours * 60 * 60) / stepSeconds);
  $: lastSeconds = data?.last ?? Math.floor(Date.now() / 1000 / stepSeconds) * stepSeconds;
  $: from = (lastSeconds - (count - 1) * stepSeconds) * 1000;
  $: step = stepSeconds * 1000;
  $: withDate = hours > 7 * 24;

  // The panel uses only these of the devices. The status refresh every 2 s brings new device objects, which would
  // otherwise redraw the charts and the table each time; these stay equal, so nothing below runs again.
  $: thermostatId = thermostat?.id;
  $: sensorId = sensor?.id;
  $: sensorName = sensor?.name;
  $: sensorHumidity = !!sensor?.humidity;

  $: thermostatHistory = thermostatId !== undefined ? data?.thermostats.find((v) => v.id === thermostatId) : undefined;
  $: sensorHistory = sensorId !== undefined ? data?.sensors.find((v) => v.id === sensorId) : undefined;
  // The values follow the devices, not the readings, so the panel keeps its size when the history arrives
  $: columns = [
    sensorName !== undefined && {
      key: "sensor",
      label: sensorName,
      color: SENSOR_COLOR,
      values: alignEnd(sensorHistory?.temperature, count),
      format: celsius,
    },
    thermostatId !== undefined && {
      key: "thermostat",
      label: "Thermostat",
      color: THERMOSTAT_COLOR,
      values: alignEnd(thermostatHistory?.temperature, count),
      format: celsius,
    },
    thermostatId !== undefined && {
      key: "target",
      label: "Target",
      color: TARGET_COLOR,
      dashed: true,
      values: alignEnd(thermostatHistory?.targetTemperature, count),
      format: (value: number) => (value ? celsius(value) : "off"),
    },
    sensorHumidity && {
      key: "humidity",
      label: "Humidity",
      color: SENSOR_COLOR,
      values: alignEnd(sensorHistory?.humidity, count),
      format: percent,
    },
    thermostatId !== undefined && {
      key: "offset",
      label: "Offset",
      color: THERMOSTAT_COLOR,
      values: alignEnd(thermostatHistory?.offset, count),
      format: signedCelsius,
    },
  ].filter((column): column is Column => !!column);
  $: empty = !!data && !columns.some((column) => hasValues(column.values));

  // Without a point under the pointer, the readout shows the newest values
  $: newest = lastIndexWithValues(columns, count);
  $: shown = hoverIndex ?? newest;

  $: temperatureSeries = columns
    .filter((column) => ["sensor", "thermostat", "target"].includes(column.key))
    .map((column) => series(column, column.key === "target"));
  $: humiditySeries = columns.filter((column) => column.key === "humidity").map((column) => series(column));
  $: offsetSeries = columns.filter((column) => column.key === "offset").map((column) => series(column, true));
  // The time axis goes under the lowest chart
  $: lowest = offsetSeries.length ? "offset" : humiditySeries.length ? "humidity" : "temperature";

  function series(column: Column, stepped = false): ChartSeries {
    return {
      ...column,
      stepped,
      // the target temperature line stops while heating is off
      values: column.key === "target" ? column.values.map((value) => value || null) : column.values,
    };
  }

  function lastIndexWithValues(columns: Column[], count: number) {
    for (let index = count - 1; index >= 0; index--) {
      if (columns.some((column) => column.values[index] != null)) {
        return index;
      }
    }
    return count - 1;
  }

  function valueAt(column: Column, index: number) {
    const value = column.values[index];
    return value == null ? "–" : column.format(value);
  }

  // Reactive, so that the charts describe the new values when the history arrives
  $: describe = (index: number) =>
    [
      formatTime(from + index * step, withDate),
      ...columns.map((column) => `${column.label} ${valueAt(column, index)}`),
    ].join(", ");

  // Newest first, without the steps that have no values
  $: rows = tableOpen
    ? Array.from({ length: count }, (_, i) => count - 1 - i).filter((index) =>
        columns.some((column) => column.values[index] != null)
      )
    : [];
</script>

<section class="history">
  <div class="heading">
    <small>History</small>
    <div role="group">
      {#each RANGES as option (option.label)}
        <button
          class="secondary"
          class:outline={option !== range}
          aria-pressed={option === range}
          on:click={() => selectRange(option)}
        >
          {option.label}
        </button>
      {/each}
    </div>
  </div>

  <!-- Drawn empty until the history arrives, so that the dialog doesn't change its height then -->
  <div class="charts" class:loading>
    <!-- The readout is the legend, with the values at the crosshair or the newest ones -->
    <p class="readout">
      <span class="time" aria-busy={!data && !failed}>
        {data ? formatTime(from + shown * step, withDate) : " "}
      </span>
      {#each columns as column (column.key)}
        <span class="item">
          <span class="key" class:dashed={column.dashed} style="--color: {column.color}"></span>
          <strong>{valueAt(column, shown)}</strong>
          <small>{column.label}</small>
        </span>
      {/each}
    </p>

    <div class="plots">
      {#if temperatureSeries.length}
        <HistoryChart
          title="Temperature, ℃"
          series={temperatureSeries}
          {from}
          {step}
          {count}
          format={(value) => value.toLocaleString()}
          {describe}
          minSpan={2}
          height={130}
          showTimeAxis={lowest === "temperature"}
          bind:hoverIndex
        />
      {/if}
      {#if humiditySeries.length}
        <HistoryChart
          title="Humidity, %"
          series={humiditySeries}
          {from}
          {step}
          {count}
          format={(value) => value.toLocaleString()}
          {describe}
          minSpan={10}
          height={70}
          showTimeAxis={lowest === "humidity"}
          bind:hoverIndex
        />
      {/if}
      {#if offsetSeries.length}
        <HistoryChart
          title="Offset, ℃"
          series={offsetSeries}
          {from}
          {step}
          {count}
          format={(value) => value.toLocaleString()}
          {describe}
          minSpan={1}
          include={0}
          height={60}
          showTimeAxis={lowest === "offset"}
          bind:hoverIndex
        />
      {/if}
      {#if empty}
        <p class="empty">
          <small>
            No readings yet. FritzGate keeps a reading every 5 minutes for 30 days, in memory, so they start over when it
            restarts.
          </small>
        </p>
      {/if}
    </div>

    <details bind:open={tableOpen}>
      <summary><small>Table</small></summary>
      {#if tableOpen}
        <div class="table">
          <table class="striped">
            <thead>
              <tr>
                <th scope="col">Time</th>
                {#each columns as column (column.key)}
                  <th scope="col">{column.label}</th>
                {/each}
              </tr>
            </thead>
            <tbody>
              {#each rows as index (index)}
                <tr>
                  <th scope="row">{formatTime(from + index * step, withDate)}</th>
                  {#each columns as column (column.key)}
                    <td>{valueAt(column, index)}</td>
                  {/each}
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
      {/if}
    </details>
  </div>

  {#if failed}
    <p class="note">
      <mark>couldn't {data ? "refresh" : "load"} the history</mark>
    </p>
  {/if}
</section>

<style>
  /* The series colors, in light and dark */
  .history {
    --history-sensor: #2a78d6;
    --history-thermostat: #eb6834;
    --history-target: #1baf7a;
    margin-top: 0.75em;
  }

  @media (prefers-color-scheme: dark) {
    :global(:root:not([data-theme="light"])) .history {
      --history-sensor: #3987e5;
      --history-thermostat: #d95926;
      --history-target: #199e70;
    }
  }

  :global([data-theme="dark"]) .history {
    --history-sensor: #3987e5;
    --history-thermostat: #d95926;
    --history-target: #199e70;
  }

  small {
    color: var(--pico-muted-color);
  }

  .heading {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    justify-content: space-between;
    gap: 0.25em 0.75em;
    margin-bottom: 0.5em;
  }

  [role="group"] {
    width: auto;
    margin: 0;
  }

  [role="group"] button {
    padding: 0.2em 0.7em;
    font-size: 0.8125em;
  }

  .charts {
    transition: opacity 0.3s;
  }

  /* While the history loads, the previous one, or the empty charts, stay, faded */
  .charts.loading {
    opacity: 0.5;
  }

  .readout {
    display: flex;
    flex-wrap: wrap;
    align-items: baseline;
    gap: 0.25em 0.9em;
    margin-bottom: 0.5em;
    font-size: 0.875em;
  }

  .time {
    flex-basis: 100%;
    color: var(--pico-muted-color);
    font-size: 0.875em;
  }

  .item {
    white-space: nowrap;
  }

  /* As wide as most values, so that a gap ("–") under the crosshair doesn't move the other values to another line;
     aligned to the right, to stay next to their labels */
  .item strong {
    display: inline-block;
    min-width: 3.2em;
    text-align: right;
    font-variant-numeric: tabular-nums;
  }

  /* A short stroke of the line's color, like the line itself */
  .key {
    display: inline-block;
    width: 1em;
    margin-right: 0.3em;
    vertical-align: middle;
    border-top: 2px solid var(--color);
  }

  .key.dashed {
    border-top-style: dashed;
  }

  .plots {
    position: relative;
  }

  .empty {
    position: absolute;
    top: 50%;
    left: 50%;
    width: min(22em, 90%);
    margin: 0;
    padding: 0.5em 0.75em;
    transform: translate(-50%, -50%);
    text-align: center;
    border-radius: var(--pico-border-radius);
    background: var(--pico-card-background-color);
  }

  details {
    margin: 0.25em 0 0;
    padding: 0;
    border: 0;
  }

  .table {
    max-height: 15em;
    overflow: auto;
  }

  table {
    margin: 0;
    font-size: 0.8125em;
    font-variant-numeric: tabular-nums;
  }

  th,
  td {
    padding: 0.25em 0.5em;
    white-space: nowrap;
  }

  .note {
    margin: 0.5em 0 0;
  }
</style>
