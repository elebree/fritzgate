<script lang="ts">
  import { onDestroy, onMount } from "svelte";

  export let onClose: () => void;

  let dialog: HTMLDialogElement;
  let pressedOnOverlay = false;

  export function close() {
    dialog?.close();
  }

  onMount(() => {
    document.documentElement.classList.add("modal-is-open");
    dialog.showModal();
  });

  onDestroy(() => {
    document.documentElement.classList.remove("modal-is-open");
  });

  // A click on the overlay around the card closes the dialog, like Esc and the close button.
  // The press must start there too, so dragging out of the card or releasing a long press doesn't close it.
  function onDialogClick(event: MouseEvent) {
    if (pressedOnOverlay && event.target === dialog) {
      close();
    }
  }
</script>

<!-- svelte-ignore a11y_click_events_have_key_events a11y_no_noninteractive_element_interactions -->
<dialog
  bind:this={dialog}
  on:close={onClose}
  on:pointerdown={(event) => (pressedOnOverlay = event.target === dialog)}
  on:click={onDialogClick}
>
  <article>
    <header>
      <button aria-label="Close" rel="prev" on:click={close}></button>
      <slot name="header" />
    </header>
    <slot />
    {#if $$slots.footer}
      <footer>
        <slot name="footer" />
      </footer>
    {/if}
  </article>
</dialog>
