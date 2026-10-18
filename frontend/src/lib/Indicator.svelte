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
</script>

<div
  bind:this={wrapper}
  bind:clientWidth={size}
  style="width: 100%;aspect-ratio: 1/1;--label-font-size: {labelFontSize}px"
>
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
    </Gauge>
  </Gauge>
</div>

<style lang="scss">
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
