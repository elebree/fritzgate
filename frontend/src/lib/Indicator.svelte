<script lang="ts">
  export let humidity: number | undefined = undefined;
  export let temperature: number;
  export let title: string = "";
  export let titleLeft: string = "";
  export let signalLeft: number | undefined = undefined;
  export let chargeLeft: number | undefined = undefined;
  export let temperatureLeft: number | undefined = undefined;
  export let targetTemperature: number | undefined = undefined;

  export let titleRight: string = "";
  export let signalRight: number | undefined = undefined;
  export let chargeRight: number | undefined = undefined;
  // false when the reading comes from the thermostat itself, so there is no separate sensor to report on
  export let showRightBadges = true;
  // Called after the gauge is pressed and held; the gauge isn't holdable without it
  export let onHold: (() => void) | undefined = undefined;

  import Gauge from "svelte-gauge";
  import heatIcon from "../assets/heating.svg";
  import settingsIcon from "../assets/settings.svg";
  import BatteryIcon from "./BatteryIcon.svelte";
  import HumidityIcon from "./HumidityIcon.svelte";
  import Icon from "./Icon.svelte";
  import SignalIcon from "./SignalIcon.svelte";
  import TemperatureIcon from "./TemperatureIcon.svelte";

  let wrapper: HTMLDivElement;
  let size = 0;
  const canvas = document.createElement("canvas").getContext("2d");

  // Scale the rings and the curved labels with the gauge, so narrow screens keep the desktop proportions
  $: stroke = Math.min(10, Math.max(6, Math.round(size / 31)));
  $: labelFontSize = Math.min(20, Math.max(10, size / 16));
  // svelte-gauge draws the labels 14px inside the edge, each centered in its own third of the circle
  $: labelWidth = (2 * Math.PI * (size / 2 - 14)) / 3 - labelFontSize;
  $: labelFont = wrapper
    ? `${labelFontSize}px ${getComputedStyle(wrapper).fontFamily}`
    : "";
  $: labels = [titleLeft, title, titleRight].map((label) =>
    fitLabel(label, labelWidth, labelFont)
  );

  // Shorten a label with an ellipsis so it doesn't run into the neighbouring one
  function fitLabel(label: string, width: number, font: string) {
    if (!canvas || !font || width <= 0) return label;
    canvas.font = font;
    if (canvas.measureText(label).width <= width) return label;
    let text = label;
    while (text && canvas.measureText(text.trimEnd() + "…").width > width) {
      text = text.slice(0, -1);
    }
    return text.trimEnd() + "…";
  }

  const HOLD_MS = 600;
  // svelte-gauge always tweens for one second, so the easing completes the fill within HOLD_MS
  const holdEasing = (t: number) => Math.min(1, (t * 1000) / HOLD_MS);
  let holding = false;
  let holdTimer: ReturnType<typeof setTimeout> | undefined;

  // A touch or pen press-and-hold also counts as a right click, which can arrive after the dialog
  // has opened under the finger (on release on Windows), so context menus are blocked until the press is over
  function blockContextMenuDuringPress() {
    const prevent = (event: Event) => event.preventDefault();
    const release = () => {
      setTimeout(() => window.removeEventListener("contextmenu", prevent, true), 500);
      window.removeEventListener("pointerup", release, true);
      window.removeEventListener("pointercancel", release, true);
    };
    window.addEventListener("contextmenu", prevent, true);
    window.addEventListener("pointerup", release, true);
    window.addEventListener("pointercancel", release, true);
  }

  function startHold(event: PointerEvent) {
    if (!onHold || event.button !== 0) return;
    blockContextMenuDuringPress();
    holding = true;
    holdTimer = setTimeout(() => {
      stopHold();
      onHold?.();
    }, HOLD_MS);
  }

  function stopHold() {
    holding = false;
    clearTimeout(holdTimer);
  }
</script>

<!-- The hold is for pointers; keyboard and screen reader users get the hidden button inside -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div
  bind:this={wrapper}
  bind:clientWidth={size}
  class:holdable={!!onHold}
  on:pointerdown={startHold}
  on:pointerup={stopHold}
  on:pointerleave={stopHold}
  on:pointercancel={stopHold}
  style="width: 100%;aspect-ratio: 1/1;--label-font-size: {labelFontSize}px"
>
  {#if onHold}
    <button class="hold-button" on:click={onHold}>Configure {title}</button>
  {/if}
  <Gauge
    class="indicator-gauge"
    bind:value={temperature}
    start={10}
    stop={30}
    {stroke}
    labelsCentered={true}
    {labels}
    segments={[[temperature, targetTemperature || temperature]]}
    --stroke-color="#f2c894"
  >
    <Gauge
      bind:value={humidity}
      {stroke}
      segments={humidity === undefined ? [] : [[40, 60]]}
      --stroke-color="#9fc0de"
    >
      <div class="slider-content">
        <span style="white-space:nowrap"
          ><TemperatureIcon value={temperature} />{temperature.toLocaleString(
            undefined,
            {
              minimumFractionDigits: 1,
              maximumFractionDigits: 1,
            }
          )}</span
        >
        {#if humidity !== undefined}
          <span style="white-space:nowrap"
            ><HumidityIcon value={humidity} />{humidity}%</span
          >
        {/if}
      </div>
      {#if targetTemperature}
        <div
          class="badge top"
          data-tooltip="Target temperature {targetTemperature}℃"
        >
          <Icon src={heatIcon} />{targetTemperature}℃
        </div>
      {/if}
      <div class="badge top left">
        <SignalIcon value={signalLeft} />
      </div>
      {#if showRightBadges}
        <div class="badge top right">
          <SignalIcon value={signalRight} />
        </div>
        <div class="badge right">
          <BatteryIcon value={chargeRight} />
        </div>
      {/if}
      <div class="badge left">
        <BatteryIcon value={chargeLeft} />
      </div>
      {#if temperatureLeft}
        <div class="badge bottom">
          {temperatureLeft?.toLocaleString(undefined, {
            minimumFractionDigits: 1,
            maximumFractionDigits: 1,
          }) + "℃"}
        </div>
      {/if}
      <div class="badge bottom left"></div>
      {#if false}
        <div class="badge bottom right">
          <img src={settingsIcon} alt="" />
        </div>
      {/if}
      {#if onHold}
        <div class="hold-progress" class:active={holding}>
          <Gauge
            value={holding ? 100 : 0}
            {stroke}
            easing={holdEasing}
            displayValue=""
            --stroke-color="var(--pico-primary-background)"
          />
        </div>
      {/if}
    </Gauge>
  </Gauge>
</div>

<style lang="scss">
  .holdable {
    cursor: pointer;
    user-select: none;
    -webkit-user-select: none;
    -webkit-touch-callout: none;
    border-radius: 50%;

    &:focus-within {
      outline: 2px solid var(--pico-primary-focus);
    }
  }

  .hold-button {
    position: absolute;
    width: 1px;
    height: 1px;
    margin: -1px;
    padding: 0;
    overflow: hidden;
    clip-path: inset(50%);
    white-space: nowrap;
    border: 0;
  }

  // Fills inside the humidity ring while a holdable gauge is pressed, and drains when released early
  .hold-progress {
    position: absolute;
    inset: 0;
    opacity: 0;
    transition: opacity 0.3s;
    pointer-events: none;

    &.active {
      opacity: 1;
    }
  }

  .slider-content {
    top: 50%;
    width: 100%;
    left: 50%;
    font-size: calc(var(--gauge-radius) / 2.5);
    font-weight: 300;
    transform: translate(-50%, -50%);
    position: absolute;
    display: grid;

    :global(.icon) {
      width: 0.45em;
      height: 0.45em;
      vertical-align: baseline;
    }
  }

  // Badges and icons scale with the inner gauge, so they keep clear of the readings on small screens
  .badge {
    opacity: 0.3;
    position: absolute;
    font-size: calc(var(--gauge-radius) / 6.5);

    :global(.icon) {
      width: 1.2em;
      height: 1.2em;
    }
  }
  .badge.top {
    left: 50%;
    top: 2%;
    transform: translate(-50%, 0);
    text-wrap: nowrap;
  }

  .badge.top.left {
    left: 8%;
    top: 13%;
    transform: none;
  }

  .badge.top.right {
    right: 0%;
    top: 13%;
    -webkit-transform: translate(20%, 0%) scaleX(-1);
    transform: translate(20%, 0%) scaleX(-1);
  }

  .badge.left {
    left: 0%;
    top: 50%;
    transform: translate(0, -50%);
  }

  .badge.right {
    right: 0%;
    top: 50%;
    transform: translate(0, -50%);
  }

  .badge.bottom {
    left: 50%;
    bottom: 2%;
    transform: translate(-50%, 0);
  }

  .badge.bottom.left {
    left: 13%;
    bottom: 13%;
    transform: none;
  }

  .badge.bottom.right {
    left: auto;
    top: auto;
    right: 15%;
    bottom: 15%;
  }

  :global(.indicator-gauge .gauge-labels) {
    font-size: var(--label-font-size);
  }
</style>
