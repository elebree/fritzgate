<script lang="ts">
  import Icon from "./Icon.svelte";

  export let undefinedIcon: string;
  export let icons: string[];
  export let min: number;
  export let max: number;
  export let value: number | undefined | null;
  export let displayValue: string;
</script>

<Icon
  src={value === undefined || value === null
    ? undefinedIcon
    : icons[Math.trunc((icons.length-0.01) * (Math.max(min, Math.min(max, value)) - min) / (max - min))]}
  label={displayValue}
/>
