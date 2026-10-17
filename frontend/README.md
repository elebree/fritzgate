# FritzGate web UI

The web interface for [FritzGate](../README.md), built with Svelte 5, TypeScript and Vite.
It shows the BLE sensors the device can see, binds FRITZ!DECT thermostats to reference
sensors, and stores the FRITZ!Box login.

The UI is **compiled into the firmware**. It is not served from flash files. A firmware
build bundles the whole app into one HTML file
([vite-plugin-singlefile](https://www.npmjs.com/package/vite-plugin-singlefile)), gzips it
and writes it as a C byte array to [`../src/index_html.h`](../src/index_html.h). The ESP32
serves that blob for every request outside `/api/`.

## Requirements

- Node.js 26 (pinned in [`.node-version`](.node-version) and [`mise.toml`](mise.toml))
- [pnpm](https://pnpm.io/)

## Getting started

```bash
pnpm install
pnpm run serve      # dev server at http://localhost:5173
```

## Scripts

| Command                   | What it does                                                                     |
| ------------------------- | -------------------------------------------------------------------------------- |
| `pnpm run serve`          | Starts the Vite dev server with HMR.                                             |
| `pnpm run build:firmware` | Builds the single-file bundle and regenerates `../src/index_html.h`.             |
| `pnpm run build`          | Builds the GitHub Pages demo into `dist/`. It does not touch the firmware.       |
| `pnpm run preview`        | Serves the contents of `dist/` locally.                                          |
| `pnpm run check`          | Type-checks the project with `svelte-check`.                                     |
| `pnpm run cy:e2e`         | Runs the Cypress e2e tests headless.                                             |
| `pnpm run cy:open`        | Opens the Cypress UI.                                                            |

## Updating the firmware UI

1. Run `pnpm run build:firmware`.
2. Rebuild and flash the firmware from the repository root: `pio run -t upload`.

`src/index_html.h` is generated. Do not edit it by hand.

## Backend: device or demo

The app talks to its backend through the `ApiService` interface in
[`src/interfaces.ts`](src/interfaces.ts). [`src/main.ts`](src/main.ts) picks an
implementation from the hostname:

- **`DemoApiService`** is used when the host ends in `.github.io`. It reads the static
  [`public/status.json`](public/status.json), adds some noise to the readings and fakes
  the write calls. This is the [live demo](https://elebree.github.io/fritzgate/).
- **`FritzGateApiService`** is used everywhere else and calls `/api/` on the device. On
  `localhost` or `127.0.0.1` it calls `http://fritzgate/api/`. To develop against a real
  board, the board must be reachable as `fritzgate` on your LAN.

The firmware API is CORS-open:

| Endpoint               | Body                    | Purpose                                         |
| ---------------------- | ----------------------- | ----------------------------------------------- |
| `GET /api/status`      | —                       | FRITZ!Box status, sensors and thermostats       |
| `POST /api/thermostat` | `{id, referenceSensor}` | Binds a thermostat to a reference sensor        |
| `POST /api/fritzauth`  | `{host, user, pass}`    | Saves the FRITZ!Box login                       |

The `StatusData`, `SensorStatus` and `DeviceStatus` types in `src/interfaces.ts` must match
the JSON that the firmware sends from `GET /api/status`. If you change one side, change the
other.

## Project layout

```text
src/
  main.ts          entry point, picks the ApiService
  App.svelte       nav, hash routing, polls the status every 2 s
  Pages/           PageSensors (#sensors), PageThermostats (#heating), PageRouterLogin (#setup)
  lib/             shared components (device list, indicators, icons, wait box)
  services/        DemoApiService and FritzGateApiService
  assets/          SVG icons
  app.scss         Pico CSS setup, plus pico-theme.scss and pico-bootstrap.scss
public/status.json sample data for the demo
cypress/           e2e specs and fixtures
```

Routing is handled by hand in `App.svelte`: it watches `location.hash`, and an unknown hash
shows the sensors page. Styles use [Pico CSS](https://picocss.com/) with Bootstrap SCSS
pieces.

## E2E tests

The Cypress specs visit `http://localhost:5173`, so start `pnpm run serve` first. They stub
`GET /api/status` with `cy.intercept` and the fixture in `cypress/fixtures/status.json`.

```bash
pnpm exec cypress install                               # once: pnpm-workspace.yaml blocks Cypress's postinstall download
pnpm run cy:e2e                                         # all specs
pnpm exec cypress run --spec cypress/e2e/sensors.cy.ts  # a single spec
```

## Deployment

On every push to `main`, [`deploy.yml`](../.github/workflows/deploy.yml) runs
`pnpm run build` and publishes `dist/` to GitHub Pages.

## IDE

Use [VS Code](https://code.visualstudio.com/) with the
[Svelte extension](https://marketplace.visualstudio.com/items?itemName=svelte.svelte-vscode).
VS Code suggests it when you open the folder.
