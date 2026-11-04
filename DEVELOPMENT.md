# FritzGate for developers

[![PlatformIO CI](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml) [![Frontend CI](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml)

This document covers building FritzGate from source, flashing it without the web installer, and how the code is organized. To install and use FritzGate, see the [README](README.md).

## How it works

- The board listens for Bluetooth advertisements from nearby sensors, decodes them with [TheengsDecoder](https://github.com/theengs/decoder) and keeps an exponential moving average of the temperature for each sensor.
- Every 30 seconds it reads the thermostats from the FRITZ!Box. For each thermostat bound to a sensor it calculates the offset as *sensor temperature − thermostat temperature*, rounds it to 0.5 °C, limits it to ±5 °C and sends it to the FRITZ!Box if it changed.
- It does not change the offset if the sensor's last reading is older than 5 minutes or if the thermostat's heating is turned off.
- If neither the FRITZ!Box nor any sensor sends updates for 5 minutes, the board restarts itself.
- The board logs in to the FRITZ!Box with the PBKDF2 challenge-response scheme of `login_sid.lua?version=2`, so it never sends the password to the FRITZ!Box. The credentials are stored in the board's EEPROM. The web interface and its API use plain HTTP without authentication.

## Supported sensors

FritzGate supports every device that the TheengsDecoder version pinned in [platformio.ini](platformio.ini) (currently 1.7.8) can decode and that reports a temperature. The [Theengs device list](https://decoder.theengs.io/devices/devices.html) follows the latest TheengsDecoder release, so it can include models that FritzGate does not support yet. FritzGate does not decrypt encrypted advertisements.

## Flashing without the web installer

The ESP32-CAM has no USB port of its own. Connect it through an ESP32-CAM-MB adapter or a USB-to-serial adapter.

### Release image

Each [GitHub release](https://github.com/elebree/fritzgate/releases) includes a complete image, `fritzgate-<version>-full.bin`. Write it to address `0x0` with [esptool](https://github.com/espressif/esptool), Espressif's Flash Download Tool or the [browser flasher](https://espressif.github.io/esptool-js/). The complete image also erases saved settings.

```bash
esptool.py --chip esp32 write_flash 0x0 fritzgate-<version>-full.bin
```

To update without losing settings, write the four separate parts instead. The release notes list the command and the addresses.

### USB-to-serial adapter

Connect GND to GND, 5V to 5V, the adapter's TX to U0R and its RX to U0T. Connect IO0 to GND, then power the board and start flashing. When flashing is done, remove the IO0 wire and press RST.

With the MB adapter, put the board into download mode if the flashing tool can't connect: hold IO0, press and release RST, then release IO0.

## Building from source

Clone the repository:

```bash
git clone https://github.com/elebree/fritzgate.git
```

### PlatformIO (recommended)

Run these commands from the repository root:

```bash
pio run                 # build
pio run -t upload       # build and flash
pio device monitor      # serial monitor, 115200 baud
```

The PlatformIO build sets the `READ_WIFI_QR_CODE` flag, so the board reads the Wi-Fi QR code on the first start. The serial monitor also prints the board's IP address.

### Arduino IDE

- Install the **esp32** board package version **2.0.x**. The PlatformIO build uses 2.0.11. Version 3.x is not supported.
- Install these libraries: [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.x, [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 1.x, [TheengsDecoder](https://github.com/theengs/decoder) 1.7.8 and [NTPClient](https://github.com/arduino-libraries/NTPClient).
- Open `src/src.ino` and select the **AI Thinker ESP32-CAM** board with the **Huge APP** partition scheme. The firmware is about 1.5 MB, which does not fit into the default app partition.
- Enter your Wi-Fi network and password in `wifi_ssid` and `wifi_pass` of the `Config` struct in `src/src.ino`. Alternatively, install [ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader) and add `#define READ_WIFI_QR_CODE` at the top of `src/src.ino` to read the Wi-Fi QR code. The code must use the standard `WIFI:S:<network>;T:WPA;P:<password>;;` format.
- Upload the sketch to your board.

## Repository layout

- `src/` is the ESP32 firmware (C++, Arduino framework, built with PlatformIO). [src/src.ino](src/src.ino) is the entry point. It runs the control loop and a small HTTP server for the web interface and the `/api/` endpoints.
- `frontend/` is the Svelte web interface. It is compiled into the firmware as `src/index_html.h`. See [frontend/README.md](frontend/README.md).
- `site/` is the [website](https://fritzgate.elebree.com/) with the [web installer](https://fritzgate.elebree.com/flasher/).
- `.github/workflows/` holds the CI builds, the release workflow and the website deployment.

## Changing the web interface

The web interface is not stored as files on the board. It is bundled into one HTML file, gzipped and compiled into the firmware as `src/index_html.h`. After you change it, run this in `frontend/`, then build and flash the firmware again:

```bash
pnpm run build:firmware
```

`src/index_html.h` is generated. Don't edit it by hand. See [frontend/README.md](frontend/README.md) for the dev server, the tests and the API.

## Website and releases

GitHub Actions publish the website on every push to `main`:

- `/` is the landing page from `site/`.
- `/flasher/` is the web installer, built on [ESP Web Tools](https://esphome.github.io/esp-web-tools/). [site/flasher/fetch-firmware.sh](site/flasher/fetch-firmware.sh) downloads the latest release during the deployment, because GitHub release downloads have no CORS headers.
- `/demo/` is the demo build of `frontend/`, which uses sample data instead of a board.

Pushing a `v*` tag builds the firmware and creates a GitHub release with the flash images. The website is then published again, so the web installer offers the new release. Tags with a `-` suffix, such as `v1.3.0-beta`, become prereleases and don't change the web installer.
