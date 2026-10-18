<script lang="ts">
  import { onMount } from "svelte";
  import iotSvg from "../assets/iot.svg";
  import { type ApiService, type StatusData, Status } from "../interfaces";
  import Dialog from "./Dialog.svelte";

  export let api: ApiService;
  export let data: StatusData;
  export let onClose: () => void;

  let dialog: Dialog;
  let host: string;
  let pass: string;
  let status: string | undefined;
  let user: string;
  // After Connect the gateway drops the old connection (CONFIGURED) and logs in with the new
  // credentials. Until it has been seen leaving, a "connected" status is still the old connection.
  let submitted = false;
  let reconnecting = false;

  $: connecting =
    submitted || status === Status.configured || status === Status.connecting;
  $: onStatus(data?.fritz?.status);

  function onStatus(fritzStatus: string | undefined) {
    status = fritzStatus;
    if (!submitted) return;
    if (status === Status.configured || status === Status.connecting) {
      reconnecting = true;
    } else if (reconnecting && status === Status.connected) {
      dialog?.close();
    } else if (reconnecting && status === Status.failure) {
      // Back to the form, which shows the failure
      submitted = reconnecting = false;
    }
  }

  async function submitForm() {
    await api.setConfig(host, user, pass);
    submitted = true;
    reconnecting = false;
  }

  onMount(() => {
    ({ host, user } = data?.fritz ?? {});
  });
</script>

<Dialog bind:this={dialog} {onClose}>
  <strong slot="header">
    {status === Status.connected && !submitted
      ? "Update FRITZ!Box credentials"
      : "Connect to FRITZ!Box"}
  </strong>

  <img src={iotSvg} class="illustration" alt="" />

  {#if connecting}
    <p>connecting to FRITZ!Box, please wait, this may take a while</p>
    <progress></progress>
  {:else}
    <p>
      Set up a hostname (in most cases <a
        href="https://fritz.box"
        target="_blank">fritz.box</a
      >
      or
      <a href="https://192.168.178.1" target="_blank">192.168.178.1</a>),
      username and password to connect to the FRITZ!Box SmartHome.
    </p>
    {#if status === Status.failure}
      <p>
        <mark>failed to connect to FRITZ!Box with the provided credentials</mark>
      </p>
    {/if}
    <form on:submit|preventDefault={submitForm} autocomplete="off">
      <label>
        Router address
        <input
          type="text"
          name="ip"
          placeholder="fritz.box or 192.168.178.1"
          required
          bind:value={host}
          autocomplete="off"
        />
      </label>
      <label>
        Username
        <input
          type="text"
          name="login"
          placeholder="FRITZ!Box username"
          required
          bind:value={user}
          autocomplete="off"
        />
      </label>
      <label>
        Password
        <input
          type="password"
          name="pass"
          placeholder="FRITZ!Box password"
          required
          bind:value={pass}
          autocomplete="off"
        />
      </label>

      <input
        type="submit"
        disabled={!host || !user || !pass}
        value="Connect FritzGate to FRITZ!Box"
      />
    </form>
  {/if}
</Dialog>

<style>
  .illustration {
    display: block;
    height: 6em;
    margin: 0 auto var(--pico-spacing);
  }

  form {
    margin-bottom: 0;
  }

  form input[type="submit"],
  progress {
    margin-bottom: 0;
  }
</style>
