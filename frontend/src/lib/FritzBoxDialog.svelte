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
  // credentials. It may log in before the next status poll, so the dialog can't rely on seeing that.
  let submitted = false;
  let updatesSinceSubmit = 0;
  // The gateway was seen connecting while the dialog is open
  let reconnecting = false;

  $: onStatus(data?.fritz?.status);

  function onStatus(fritzStatus: string | undefined) {
    status = fritzStatus;
    if (status === Status.configured || status === Status.connecting) {
      reconnecting = true;
      return;
    }
    // Polls run one at a time, so only the first status after Connect can come from a request
    // sent before it, describing the old connection
    if (submitted && !reconnecting && ++updatesSinceSubmit < 2) return;
    if (!submitted && !reconnecting) return;

    if (status === Status.connected) {
      dialog?.close();
    } else {
      // Back to the form, which shows a failure
      submitted = reconnecting = false;
    }
  }

  async function submitForm() {
    await api.setConfig(host, user, pass);
    submitted = true;
    updatesSinceSubmit = 0;
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

  <!-- Checked here rather than in a $: value, which could run before onStatus() sets the status -->
  {#if submitted || status === Status.configured || status === Status.connecting}
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
