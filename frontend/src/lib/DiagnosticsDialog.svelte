<script lang="ts">
  import { onDestroy, onMount, tick } from "svelte";
  import type { ApiService, Diagnostics } from "../interfaces";
  import Dialog from "./Dialog.svelte";

  export let api: ApiService;
  export let onClose: () => void;

  // Why the board started last, by the key of GET /api/diagnostics
  const RESET_REASONS: Record<string, string> = {
    power_on: "power on, reset button or flashing",
    external: "reset pin",
    software: "restarted by the firmware",
    crash: "crash of the firmware",
    task_watchdog: "watchdog: no thermostat data from the FRITZ!Box for 5 minutes",
    interrupt_watchdog: "watchdog: the firmware hung",
    watchdog: "watchdog",
    brownout: "supply voltage too low, try another power supply or USB cable",
    deep_sleep: "wake from deep sleep",
    sdio: "SDIO reset",
  };
  // Level letter of a log line, such as W in "[  12345][W] message"
  const LEVEL = /^\[\s*\d+\]\[([A-Z])\]/;
  // The dialog asks for the state and the new log lines every few seconds
  const POLL_MS = 2000;
  // Log lines the dialog keeps while it is open; the board keeps a few hundred
  const MAX_LINES = 2000;

  let diagnostics: Diagnostics | undefined = undefined;
  let lines: string[] = [];
  // Number of the next log line to ask for
  let next = 0;
  let failed = false;
  let logElement: HTMLElement;
  // Only the answer to the latest request counts, and none after the dialog closed
  let request = 0;
  let timer: ReturnType<typeof setTimeout> | undefined;

  async function poll() {
    clearTimeout(timer);
    const current = ++request;
    let delay = POLL_MS;
    try {
      const result = await api.fetchDiagnostics(next);
      if (current !== request) return;
      failed = false;
      const previous = diagnostics;
      diagnostics = result;
      if (previous && result.uptime < previous.uptime) {
        // FritzGate restarted: keep the lines from before, and ask for the lines since the start right away
        await addLines(["FritzGate restarted"]);
        next = 0;
        delay = 0;
        return;
      }
      const skipped = result.log.first - next;
      next = result.log.next;
      await addLines([
        ...(previous && skipped > 0 ? [`${skipped} lines not kept by FritzGate`] : []),
        ...result.log.lines,
      ]);
    } catch {
      if (current === request) failed = true;
    } finally {
      if (current === request) timer = setTimeout(poll, delay);
    }
  }

  async function addLines(added: string[]) {
    if (added.length === 0) return;
    // follow the newest lines, unless they were scrolled up to read older ones
    const following = !logElement || logElement.scrollTop + logElement.clientHeight >= logElement.scrollHeight - 8;
    lines = [...lines, ...added].slice(-MAX_LINES);
    if (following) {
      await tick();
      if (logElement) logElement.scrollTop = logElement.scrollHeight;
    }
  }

  onMount(poll);
  onDestroy(() => {
    request++;
    clearTimeout(timer);
  });

  const kilobytes = new Intl.NumberFormat(undefined, { maximumFractionDigits: 0 });

  function formatUptime(seconds: number) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    if (days > 0) return `${days} d ${hours} h`;
    if (hours > 0) return `${hours} h ${minutes} min`;
    if (minutes > 0) return `${minutes} min`;
    return `${seconds} s`;
  }

  const formatBytes = (bytes: number) => `${kilobytes.format(bytes / 1024)} KB`;

  function formatSignal(rssi: number) {
    if (!rssi) return "not connected";
    const quality = rssi >= -67 ? "good" : rssi >= -75 ? "fair" : "weak";
    return `${rssi} dBm, ${quality}`;
  }

  function rows(d: Diagnostics): [string, string][] {
    return [
      ["Firmware", d.version],
      ["Uptime", formatUptime(d.uptime)],
      ["Last start", RESET_REASONS[d.resetReason] ?? d.resetReason],
      ["Free memory", `${formatBytes(d.freeHeap)}, lowest ${formatBytes(d.minFreeHeap)}, PSRAM ${formatBytes(d.freePsram)}`],
      ["Wi-Fi signal", formatSignal(d.rssi)],
      ["FRITZ!OS", d.fritzOsVersion || "unknown"],
      ["FRITZ!Box API", d.fritzApi ?? "not logged in"],
    ];
  }

  function level(line: string) {
    return LEVEL.exec(line)?.[1] ?? "";
  }
</script>

<Dialog {onClose} wide>
  <svelte:fragment slot="header">
    <strong>Diagnostics</strong>
  </svelte:fragment>

  {#if diagnostics}
    <table class="facts">
      <tbody>
        {#each rows(diagnostics) as [label, value] (label)}
          <tr>
            <th scope="row">{label}</th>
            <td>{value}</td>
          </tr>
        {/each}
      </tbody>
    </table>

    <small class="label">Log, newest last</small>
    <!-- Lines of the board start with their time; the others are the dialog's notes, such as a restart -->
    <pre class="log" bind:this={logElement}>{#each lines as line}<span
          class="line level-{level(line)}"
          class:marker={!line.startsWith("[")}>{line}</span
        >{"\n"}{:else}<span class="line">no log lines</span>{/each}</pre>
  {:else if !failed}
    <p aria-busy="true">loading the diagnostics</p>
  {/if}
  {#if failed}
    <p class="note">
      <mark>couldn't {diagnostics ? "refresh" : "load"} the diagnostics, trying again</mark>
    </p>
  {/if}
</Dialog>

<style>
  small {
    color: var(--pico-muted-color);
  }

  .facts {
    margin-bottom: 1em;
    font-size: 0.875em;
  }

  .facts th {
    white-space: nowrap;
    color: var(--pico-muted-color);
    font-weight: normal;
  }

  .label {
    display: block;
    margin-bottom: 0.25em;
  }

  .log {
    max-height: 18rem;
    overflow: auto;
    margin: 0;
    padding: 0.5em 0.75em;
    font-size: 0.75em;
    white-space: pre-wrap;
    overflow-wrap: anywhere;
  }

  .level-E {
    color: var(--pico-del-color);
  }

  .level-W {
    color: var(--pico-primary);
  }

  .level-S {
    color: var(--pico-ins-color);
  }

  .level-H,
  .level-D {
    color: var(--pico-muted-color);
  }

  .marker {
    font-style: italic;
    color: var(--pico-muted-color);
  }

  .note {
    margin: 0.5em 0 0;
  }
</style>
