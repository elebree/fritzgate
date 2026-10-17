# FritzGate 
[![en](https://img.shields.io/badge/lang-en-blue.svg)](README.md) [![de](https://img.shields.io/badge/lang-de-red.svg)](README.de.md) [![PlatformIO CI](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml) [![Frontend CI](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml)

**FritzGate** turns an **Ai Thinker** board into a smart thermostat controller. It works with thermostats connected to a **FRITZ!Box** (`FRITZ!DECT 301` and `FRITZ!DECT 302`) and uses any of 70+ Bluetooth temperature sensors for precise room temperature control. Supported vendors include Govee, Xiaomi, Qingping, Ruuvi, Inkbird, SwitchBot and ThermoPro.
[![FritzGate screenshot](docs/FritzGate_Screenshot.svg)](https://elebree.github.io/fritzgate/)

## Demo

View the online demo [here](https://elebree.github.io/fritzgate/).

## Features

- **FRITZ!Box integration:** FritzGate reads your thermostats directly from the FRITZ!Box. It changes only the temperature offset and leaves weekly plans and all other thermostat settings alone.
- **External temperature sensors:** A thermostat measures the temperature at the radiator. Bind it to a sensor placed in the room, and FritzGate keeps correcting the thermostat's temperature offset so the room reaches the temperature you set.
- **Embedded web interface:** Monitor temperature, humidity, battery and signal of nearby sensors, and manage thermostat-sensor bindings in the browser.

## Advantages

- **Easy configuration:** Set up Wi-Fi by showing a Wi-Fi QR code to the board's camera.
- **Cost-effective hardware:** The **ESP32-CAM** board is cheap and energy efficient.
- **Powered by FRITZ!Box:** Power the board from the **FRITZ!Box** USB port. No extra space or power supply is needed.
- **70+ supported sensors:** Use any of [70+ models](https://decoder.theengs.io/devices/devices.html) of temperature sensors for comfortable rooms and energy savings.
- **Works without FRITZ!Box:** Without FRITZ!Box access you can still monitor temperature and humidity from nearby sensors.

## How it works

- The board listens for Bluetooth advertisements from nearby sensors and keeps a smoothed temperature for each one.
- Every 30 seconds it reads the thermostats from the FRITZ!Box. For each thermostat bound to a sensor it calculates the offset as *sensor temperature − thermostat temperature*, rounds it to 0.5 °C, limits it to ±5 °C and sends it to the FRITZ!Box if it changed.
- It does not change the offset if the sensor's last reading is older than 5 minutes or if the thermostat's heating is turned off.
- If neither the FRITZ!Box nor any sensor sends updates for 5 minutes, the board restarts itself.

## Getting Started

### Hardware Requirements

- Ai Thinker ESP32-CAM board, plus a USB-to-serial adapter (or an ESP32-CAM-MB board) to flash it
- [Temperature sensors](https://decoder.theengs.io/devices/devices.html)
- FRITZ!Box with compatible thermostats (optional)

### Installation

1. Clone the repository to your local machine:

    ```bash
    git clone https://github.com/elebree/fritzgate.git
    ```

2. Build and upload the firmware with [PlatformIO](https://platformio.org/) (recommended) or the [Arduino IDE](https://www.arduino.cc/en/software).

3. **PlatformIO:**
   - Upload the firmware from the repository root:

     ```bash
     pio run -t upload
     ```

   - On the first start, show a Wi-Fi QR code to the board's camera. The code must use the standard `WIFI:S:<network>;T:WPA;P:<password>;;` format. The FRITZ!Box user interface and most phones can show such a code for your network. The board saves the credentials and restarts. The flash LED blinks while the board connects to Wi-Fi.

4. **Arduino IDE:**
   - Install the **esp32** board package version **2.0.x**. The PlatformIO build uses 2.0.11. Version 3.x is not supported.
   - Install these libraries: [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.x, [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 1.x, [TheengsDecoder](https://github.com/theengs/decoder) 1.7.8 and [NTPClient](https://github.com/arduino-libraries/NTPClient).
   - Open `src/src.ino` and select the **AI Thinker ESP32-CAM** board with the **Huge APP** partition scheme. The firmware is about 1.5 MB, which does not fit into the default app partition.
   - Enter your Wi-Fi network and password in `wifi_ssid` and `wifi_pass` of the `Config` struct in `src/src.ino`. Alternatively, install [ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader) and add `#define READ_WIFI_QR_CODE` at the top of `src/src.ino` to read the Wi-Fi QR code as described above.
   - Upload the sketch to your board.

5. Open the web interface at [http://fritzgate](http://fritzgate), or at the IP address your router assigned to the board. The serial monitor (115200 baud) also prints the address.

6. Go to **Settings** and connect to your FRITZ!Box. Enter the host (in most cases `fritz.box`), and the user name and password of a FRITZ!Box user with Smart Home access.

7. On the **Settings** page, select a reference sensor for each thermostat.

## Compatible Sensors

FritzGate works with every device that [TheengsDecoder](https://github.com/theengs/decoder) v1.7.8 can decode and that reports a temperature. Some examples:

- **April Brother**: [ABN03](https://decoder.theengs.io/devices/ABN03.html), [ABN07](https://decoder.theengs.io/devices/ABN07.html), [ABTemp](https://decoder.theengs.io/devices/ABTemp.html)
- **Aranet**: [Aranet4](https://decoder.theengs.io/devices/Aranet4.html)
- **BlueMaestro**: [TD1in1](https://decoder.theengs.io/devices/BM1in1.html), [TD3in1](https://decoder.theengs.io/devices/BM3in1.html), [TD4in1](https://decoder.theengs.io/devices/BM4in1.html)
- **ClearGrass/Qingping**: [CGC1](https://decoder.theengs.io/devices/CGC1.html), [CGD1](https://decoder.theengs.io/devices/CGD1.html), [CGG1](https://decoder.theengs.io/devices/CGG1.html), [CGP1W](https://decoder.theengs.io/devices/CGP1W.html), [CGP23W](https://decoder.theengs.io/devices/CGP23W.html)
- **Govee**: [H5072](https://decoder.theengs.io/devices/H5072.html), [H5074](https://decoder.theengs.io/devices/H5074.html), [H5075](https://decoder.theengs.io/devices/H5075.html), [H5100](https://decoder.theengs.io/devices/H5100.html), [H5101](https://decoder.theengs.io/devices/H5101.html), [H5102](https://decoder.theengs.io/devices/H5102.html), [H5104](https://decoder.theengs.io/devices/H5104.html), [H5105](https://decoder.theengs.io/devices/H5105.html), [H5106](https://decoder.theengs.io/devices/H5106.html), [H5174](https://decoder.theengs.io/devices/H5174.html), [H5177](https://decoder.theengs.io/devices/H5177.html), [H5179](https://decoder.theengs.io/devices/H5179.html)
- **Inkbird**: [IBS-P01B](https://decoder.theengs.io/devices/IBS-P01B.html), [IBS-TH1](https://decoder.theengs.io/devices/IBS_TH1.html), [IBS-TH2](https://decoder.theengs.io/devices/IBS_TH2.html), [IBT_2X(S)](https://decoder.theengs.io/devices/IBT_2X.html), [IBT_4X(S/C)](https://decoder.theengs.io/devices/IBT_4XS.html), [IBT_6X(S)](https://decoder.theengs.io/devices/IBT_6XS.html), [ITH-12S](https://decoder.theengs.io/devices/ITH_12S.html)
- **Jaalee**: [F525/F51C](https://decoder.theengs.io/devices/JAALEE.html)
- **MOKOSMART**: [MBXPRO](https://decoder.theengs.io/devices/MBXPRO.html)
- **Oria/Brifit/SigmaWit/SensorPro**: [T201](https://decoder.theengs.io/devices/T201.html), [T301](https://decoder.theengs.io/devices/T301.html)
- **Qingping**: [CGDK2](https://decoder.theengs.io/devices/CGDK2.html), [CGDN1](https://decoder.theengs.io/devices/CGDN1.html)
- **rbaron**: [BPv1.0-1.2](https://decoder.theengs.io/devices/BPARASITE.html)
- **Ruuvi**: [RuuviTag](https://decoder.theengs.io/devices/RuuviTag_RAWv2.html)
- **Sensirion**: [SCD4X](https://decoder.theengs.io/devices/SCD4X.html), [SHT4X](https://decoder.theengs.io/devices/SHT4x.html)
- **SensorBlue/Oria/Brifit**: [WS02/WS08](https://decoder.theengs.io/devices/ThermoBeacon.html)
- **SwitchBot**: [Meter (Plus)](https://decoder.theengs.io/devices/SBMT.html), [Outdoor Meter](https://decoder.theengs.io/devices/SBOT.html)
- **Tenergy**: [SOLIS_6](https://decoder.theengs.io/devices/SOLIS_6.html)
- **ThermoPro**: [TP357](https://decoder.theengs.io/devices/TP357.html), [TP358](https://decoder.theengs.io/devices/TP358.html), [TP359](https://decoder.theengs.io/devices/TP359.html), [TP393](https://decoder.theengs.io/devices/TP393.html)
- **Xiaomi**: [HHCCJCY10](https://decoder.theengs.io/devices/HHCCJCY10.html), [LYWSD03MMC_ATC/PVVX](https://decoder.theengs.io/devices/LYWSD03MMC.html), [LYWSDCGQ](https://decoder.theengs.io/devices/LYWSDCGQ.html), [MHO/MMC-C401_ATC/PVVX](https://decoder.theengs.io/devices/MHO-C401.html), [MJWSD05MMC_ATC/PVVX](https://decoder.theengs.io/devices/MJWSD05MMC.html)
- **Xiaomi/Mijia**: [LYWSD02](https://decoder.theengs.io/devices/LYWSD02.html)
- **Xiaomi/VegTrug**: [HHCCJCY01HHCC](https://decoder.theengs.io/devices/HHCCJCY01HHCC.html)

The [list of Theengs-compatible devices](https://decoder.theengs.io/devices/devices.html) follows the latest TheengsDecoder release. It can include newer models, such as the Shelly BLU H&T or the Govee H5108, that FritzGate does not support yet.

## Development

The repository contains two parts that ship as one firmware image:

- `src/` is the ESP32 firmware (C++, Arduino framework, built with PlatformIO).
- `frontend/` is the Svelte web interface. It is compiled into the firmware as `src/index_html.h`.

Firmware commands, run from the repository root:

```bash
pio run                 # build
pio run -t upload       # build and flash
pio device monitor      # serial monitor, 115200 baud
```

After you change the web interface, run `pnpm run build:firmware` in `frontend/` to regenerate `src/index_html.h`, then build and flash the firmware again. See [frontend/README.md](frontend/README.md) for details.

## License

FritzGate is released under the [MIT License](LICENSE).
