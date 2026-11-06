<script lang="ts">
  import { linePath, timeTicks, valueTicks, type ChartSeries } from "./chart";

  export let title: string;
  export let series: ChartSeries[];
  /** Milliseconds of the first point, and between two points */
  export let from: number;
  export let step: number;
  export let count: number;
  /** Formats a value axis tick */
  export let format: (value: number) => string;
  /** Describes a point to screen readers */
  export let describe: (index: number) => string;
  /** The value axis spans at least this much, so that noise doesn't look like a swing */
  export let minSpan = 1;
  /** A value the value axis always shows, such as 0 */
  export let include: number | undefined = undefined;
  export let height = 100;
  export let showTimeAxis = false;
  /** The point under the pointer or chosen with the arrow keys, shared by the charts of a panel */
  export let hoverIndex: number | undefined = undefined;

  // Room for the value axis labels on the left, and for the time axis labels at the bottom
  const LEFT = 34;
  const RIGHT = 6;
  const TOP = 6;
  const TIME_AXIS = 18;

  let width = 0;
  $: bottom = showTimeAxis ? TIME_AXIS : 4;
  $: plotWidth = Math.max(0, width - LEFT - RIGHT);
  $: plotHeight = height - TOP - bottom;

  $: values = series.flatMap((line) => line.values.filter((value): value is number => value != null));
  $: axis = valueAxis(values, minSpan, include, plotHeight);
  $: duration = Math.max(1, (count - 1) * step);
  $: times = timeTicks(from, from + duration, Math.max(2, Math.floor(plotWidth / 64)));
  $: shown = hoverIndex !== undefined && hoverIndex < count ? hoverIndex : undefined;

  function valueAxis(values: number[], minSpan: number, include: number | undefined, plotHeight: number) {
    let min = values.length ? Math.min(...values) : 0;
    let max = values.length ? Math.max(...values) : minSpan;
    if (max - min < minSpan) {
      const middle = (min + max) / 2;
      min = middle - minSpan / 2;
      max = middle + minSpan / 2;
    }
    if (include !== undefined) {
      min = Math.min(min, include);
      max = Math.max(max, include);
    }
    return valueTicks(min, max, Math.max(2, Math.floor(plotHeight / 28)));
  }

  $: x = (index: number) => LEFT + (count > 1 ? index / (count - 1) : 1) * plotWidth;
  $: xAt = (time: number) => LEFT + ((time - from) / duration) * plotWidth;
  $: y = (value: number) => TOP + ((axis.high - value) / (axis.high - axis.low || 1)) * plotHeight;

  function pointAt(event: PointerEvent) {
    const box = (event.currentTarget as HTMLElement).getBoundingClientRect();
    const position = (event.clientX - box.left - LEFT) / Math.max(1, plotWidth);
    hoverIndex = Math.max(0, Math.min(count - 1, Math.round(position * (count - 1))));
  }

  function onKeydown(event: KeyboardEvent) {
    const last = count - 1;
    const current = shown ?? last;
    const moves: Record<string, number> = {
      ArrowLeft: current - 1,
      ArrowRight: current + 1,
      PageUp: current - 12,
      PageDown: current + 12,
      Home: 0,
      End: last,
    };
    if (event.key === "Escape") {
      hoverIndex = undefined;
    } else if (event.key in moves) {
      hoverIndex = Math.max(0, Math.min(last, moves[event.key]));
    } else {
      return;
    }
    event.preventDefault();
  }
</script>

<figure>
  <figcaption>{title}</figcaption>
  <!-- A slider over time for keyboard and screen reader users; the readout above the charts shows the values -->
  <div
    class="plot"
    bind:clientWidth={width}
    style="height: {height}px"
    role="slider"
    tabindex="0"
    aria-label="{title}, use the arrow keys to go through time"
    aria-valuemin={0}
    aria-valuemax={Math.max(0, count - 1)}
    aria-valuenow={shown ?? Math.max(0, count - 1)}
    aria-valuetext={describe(shown ?? count - 1)}
    on:pointerdown={pointAt}
    on:pointermove={pointAt}
    on:pointerleave={() => (hoverIndex = undefined)}
    on:keydown={onKeydown}
    on:blur={() => (hoverIndex = undefined)}
  >
    {#if width > 0}
      <svg {width} {height} aria-hidden="true">
        {#each axis.ticks as tick (tick)}
          <line class="grid" x1={LEFT} x2={LEFT + plotWidth} y1={y(tick)} y2={y(tick)} />
          <!-- without values, such as while the history loads, the grid has no scale yet -->
          {#if values.length}
            <text class="axis" x={LEFT - 6} y={y(tick)} text-anchor="end" dominant-baseline="middle">
              {format(tick)}
            </text>
          {/if}
        {/each}
        {#each times as tick (tick.time)}
          <line class="grid" x1={xAt(tick.time)} x2={xAt(tick.time)} y1={TOP} y2={TOP + plotHeight} />
          {#if showTimeAxis}
            <text class="axis" x={xAt(tick.time)} y={height - 4} text-anchor="middle">{tick.label}</text>
          {/if}
        {/each}
        {#each series as line (line.key)}
          <path
            class="line"
            class:dashed={line.dashed}
            d={linePath(line.values, x, y, line.stepped)}
            stroke={line.color}
          />
        {/each}
        {#if shown !== undefined}
          <line class="crosshair" x1={x(shown)} x2={x(shown)} y1={TOP} y2={TOP + plotHeight} />
          {#each series as line (line.key)}
            {@const value = line.values[shown]}
            {#if value != null}
              <circle class="dot" cx={x(shown)} cy={y(value)} r="4" fill={line.color} />
            {/if}
          {/each}
        {/if}
      </svg>
    {/if}
  </div>
</figure>

<style>
  figure {
    margin: 0 0 0.5rem;
  }

  figcaption {
    padding: 0;
    color: var(--pico-muted-color);
    font-size: 0.75rem;
  }

  .plot {
    position: relative;
    cursor: crosshair;
    /* horizontal drags move the crosshair, vertical ones still scroll the dialog */
    touch-action: pan-y;
    border-radius: 0.25rem;
  }

  .plot:focus-visible {
    outline: 2px solid var(--pico-primary-focus);
  }

  svg {
    display: block;
    overflow: visible;
  }

  .grid {
    stroke: var(--pico-muted-border-color);
    stroke-width: 1;
    shape-rendering: crispEdges;
  }

  .axis {
    fill: var(--pico-muted-color);
    font-size: 0.6875rem;
    font-variant-numeric: tabular-nums;
  }

  .line {
    fill: none;
    stroke-width: 2;
    stroke-linejoin: round;
    stroke-linecap: round;
  }

  .dashed {
    stroke-dasharray: 5 4;
  }

  .crosshair {
    stroke: var(--pico-muted-color);
    stroke-width: 1;
    shape-rendering: crispEdges;
  }

  /* the ring in the dialog's color keeps a dot clear of the lines it sits on */
  .dot {
    stroke: var(--pico-card-background-color);
    stroke-width: 2;
  }
</style>
