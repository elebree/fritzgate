<script lang="ts">
  export let src: string;
  export let label: string = "";
</script>

<!-- The SVG is used as a mask, so the icon takes the current text color in light and dark themes -->
<span
  class="icon"
  role={label ? "img" : undefined}
  aria-hidden={label ? undefined : "true"}
  aria-label={label || undefined}
  title={label || undefined}
  style:--icon={`url("${src}")`}
></span>

<style>
  .icon {
    display: inline-block;
    width: var(--icon-size, 1.2em);
    height: var(--icon-size, 1.2em);
    vertical-align: -0.2em;
    background-color: currentColor;
    -webkit-mask: var(--icon) center / contain no-repeat;
    mask: var(--icon) center / contain no-repeat;
  }
</style>
