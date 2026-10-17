<script lang="ts">
  import { onMount } from "svelte";
  import iotSvg from "../assets/iot.svg";
  import WaitBox from "../lib/WaitBox.svelte";
  import { type ApiService, type StatusData, Status } from "../interfaces";

  export let api: ApiService;
  export let data: StatusData;

  let host: string;
  let pass: string;
  let status: string;
  let user: string;

  let submitForm = async (e: any): Promise<boolean> => {
    await api.setConfig(host, user, pass);
    status = Status.configured;
    return false;
  };

  $: {
    if (
      status &&
      status !== data?.fritz?.status &&
      data?.fritz?.status === Status.connected
    ) {
      location.hash = "#heating";
    } else {
      status = data?.fritz?.status;
      host ??= data?.fritz?.host;
      user ??= data?.fritz?.user;
    }
  }

  onMount(async function () {
    ({ host, user } = data?.fritz);
  });
</script>

{#if status === Status.configured || status === Status.connecting}
  <WaitBox
    message="connecting to FRITZ!Box"
    details="please wait, this may take a while"
  />
{:else}
  <article>
    <div class="row">
      <div class="col-12 col-md-6 col-lg-6 col-xl-6">
        <hgroup>
          <div class="message-splash">
            <img src={iotSvg} class="wait-image" alt="" />
            <h4>
              {status === Status.connected
                ? "update credentials for FRITZ!Box"
                : "connect to FRITZ!Box"}
            </h4>
          </div>
        </hgroup>
      </div>
      <div class="col-12 col-md-6 col-lg-6 col-xl-6">
        <hgroup>
          <div class="message-splash">
            <p>
              Set up a hostname (in most cases <a
                href="https://fritz.box"
                target="_blank">fritz.box</a
              >
              or
              <a href="https://192.168.178.1" target="_blank">192.168.178.1</a
              >), username and password to connect to the FRITZ!Box SmartHome.
            </p>
          </div>
        </hgroup>
        {#if status === Status.failure}
          <center
            ><p>
              <mark
                >failed to connect to FRITZ!Box with the provided credentials</mark
              >
            </p></center
          >
        {/if}
        <form
          class="fritzform"
          on:submit|preventDefault={submitForm}
          autocomplete="off"
        >
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
      </div>
    </div>
  </article>
{/if}
