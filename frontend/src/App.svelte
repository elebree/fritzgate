<script lang="ts">
  import "./app.scss";

  import PageRouterLogin from "./Pages/PageRouterLogin.svelte";
  import PageSensors from "./Pages/PageSensors.svelte";
  import PageThermostats from "./Pages/PageThermostats.svelte";
  import WaitBox from "./lib/WaitBox.svelte";
  import type { ApiService, StatusData } from "./interfaces";
  import github from "./assets/github-mark.svg";

  export let api: ApiService;
  let state: StatusData | undefined = undefined;
  let page: string = location.hash;
  let busy = false;
  let timer: NodeJS.Timeout;

  function routeChange() {
    console.log(location.hash);
    page = location.hash;
  }

  function requestNotifications() {
    Notification.requestPermission(function (result) {
      if (result === "granted") {
        // new Notification("Notifications permission granted")
      }
    });
  }

  async function fetchData() {
    try {
      const newData: StatusData | undefined = await api.fetchStatus();
      if (!newData) {
        busy = true;
        return;
      }

      if (state) {
        var newSensors = newData.sensors.filter(
          (v) => !state?.sensors.find((ns) => ns.name == v.name)
        );
        if (newSensors.length > 0) {
          new Notification(
            "Found a new sensor(s): " + newSensors.map((v) => v.name).join(", ")
          );
        }
      }

      state = {
        fritz: { ...newData.fritz },
        sensors: newData.sensors.map((v) => ({
          ...v,
          rssi: v.rssi ? v.rssi : undefined!,
        })),
        thermostats: newData.thermostats.map((v) => ({ ...v })),
      };

      busy = false;
    } catch {
      busy = true;
      // request failed
    } finally {
      timer = setTimeout(fetchData, 2000);
    }
  }
  fetchData();
</script>

<svelte:window on:hashchange={routeChange} />
<main>
  <nav>
    <ul>
      <li>
        <strong>FritzGate</strong>
        <span aria-busy={busy}></span>
      </li>
    </ul>
    <ul>
      <!-- <li><button on:click={requestNotifications}>Notifications</button></li> -->
      <li>
        <a class="secondary" href={"#sensors"}>Dashboard</a>
      </li>
      <li>
        <a class="secondary" href={"#heating"}>Settings</a>
      </li>
      <li>
        <a
          class="secondary"
          href="https://github.com/elebree/fritzgate"
          target="_blank"
          ><img
            src={github}
            alt="Github"
            style="height: 1.5em; width: 1.5em;"
          /></a
        >
      </li>
    </ul>
  </nav>
  {#if !state}
    <WaitBox
      message="trying to connect to the FritzGate"
      details="this buddy gossips too much with sensors"
    />
  {:else if page === "#sensors"}<PageSensors data={state} />
  {:else if page === "#heating"}<PageThermostats {api} data={state} />
  {:else if page === "#setup"}<PageRouterLogin {api} data={state} />
  {:else}<PageSensors data={state} />
  {/if}
</main>
