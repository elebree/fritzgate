# FritzGate 
[![en](https://img.shields.io/badge/lang-en-blue.svg)](README.md) [![de](https://img.shields.io/badge/lang-de-red.svg)](README.de.md) [![PlatformIO CI](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml) [![Frontend CI](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml)

**FritzGate** transforms your **Ai Thinker board** into a smart thermostat controller. This project enables seamless control of thermostats connected to a **FRITZ!Box** (`FRITZ!DECT 301` and `FRITZ!DECT 302`), integrating for precise temperature control any of 70+ temperature sensors from leading vendors such as Govee, Xiaomi, Shelly, Ruuvi, Inkbird, SwitchBot, and ThermoPro.
[![FritzGate screenshot](docs/FritzGate_Screenshot.svg)](https://elebree.github.io/fritzgate/)

## Demo

View the online demo [here](https://elebree.github.io/fritzgate/).

## Features

- **FRITZ!Box integration:** FritzGate does not affect weekly plans or other thermostat settings. Connect and control thermostats directly from FritzBox.
- **Binding with external Temperature Sensors:** Utilize third party temperature sensors placed in living spaces for precise temperature control in a room. FritzGate automatically corrects temperature offsets of thermostat sensors.
- **Embedded Web Server:** Easily manage thermostat-sensor bindings, monitor temperature and humidity through a user-friendly web interface.

## Advantages

- **Easy Configuration:** Set up effortlessly by scanning a QR code with the Ai Thinker camera for Wi-Fi credentials.
- **Cost-Effective Hardware:** Utilize the budget-friendly and energy efficient **ESP32-CAM** board.
- **Powered by FRITZ!Box:** Leverage the convenience of powering the device directly from the *FRITZ!Box** USB port, eliminating the need for additional space and power sources.
- **70+ Supported Sensors:** Use any of [70+ models](https://decoder.theengs.io/devices/devices.html) of temperature sensors for precise climate control in living spaces, ensuring optimal comfort and energy savings.
- **Embedded Web Server:** Easily manage thermostat-sensor bindings through the embedded web server, providing an intuitive interface for users to customize and monitor their smart heating system.
- **Can be used without FRITZ!Box:** If you're not authorized in FritzBox, you can still monitor temperature and humidity from nearby temperature sensors.

## Getting Started

### Hardware Requirements

- Ai Thinker ESP32-CAM board
- [Temperature sensors](https://decoder.theengs.io/devices/devices.html)
- Fritzbox with compatible thermostats (optional)

### Installation

1. Clone the repository to your local machine:

    ```bash
    git clone https://github.com/elebree/fritzgate.git
    ```

2. Open the project in [PlatformIO](https://platformio.org/) or [Arduino IDE](https://www.arduino.cc/en/software), depending on your preference.

3. **For PlatformIO Users:**
   - Upload the code to your Ai Thinker ESP32-CAM board.
   - After the first start, use the Ai Thinker ESP32-CAM board camera to scan the Wi-Fi QR code. This will configure the Wi-Fi credentials automatically.

4. **For Arduino IDE Users:**
   - Ensure you have  [NTPClient](https://github.com/arduino-libraries/NTPClient) and [ArduinoJson](https://github.com/bblanchon/ArduinoJson) Arduino libraries installed before building the project.
   - Set Wi-Fi credentials directly in the code (alternatively, you can install the [ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader) library from Github to be able to read the WiFi QR code).
   - Upload the code to your Ai Thinker ESP32-CAM board.

5. Access the web interface by navigating to the IP address assigned by the router, or by using the link [http://fritzgate](http://fritzgate).

## Compatible Sensors

- **April Brother**: [ABN03](https://decoder.theengs.io/devices/ABN03.html), [ABN07](https://decoder.theengs.io/devices/ABN07.html), [ABTemp](https://decoder.theengs.io/devices/ABTemp.html)
- **BlueMaestro**: [TD1in1](https://decoder.theengs.io/devices/BM1in1.html), [TD3in1](https://decoder.theengs.io/devices/BM3in1.html), [TD4in1](https://decoder.theengs.io/devices/BM4in1.html)
- **ClearGrass/Qingping**: [CGC1](https://decoder.theengs.io/devices/CGC1.html), [CGD1](https://decoder.theengs.io/devices/CGD1.html), [CGG1](https://decoder.theengs.io/devices/CGG1.html), [CGP1W](https://decoder.theengs.io/devices/CGP1W.html), [CGP23W](https://decoder.theengs.io/devices/CGP23W.html)
- **Govee**: [H5072](https://decoder.theengs.io/devices/H5072.html), [H5074](https://decoder.theengs.io/devices/H5074.html), [H5075](https://decoder.theengs.io/devices/H5075.html), [H5100](https://decoder.theengs.io/devices/H5100.html), [H5101](https://decoder.theengs.io/devices/H5101.html), [H5102](https://decoder.theengs.io/devices/H5102.html), [H5104](https://decoder.theengs.io/devices/H5104.html), [H5105](https://decoder.theengs.io/devices/H5105.html), [H5106](https://decoder.theengs.io/devices/H5106.html), [H5108](https://decoder.theengs.io/devices/H5108.html), [H5174](https://decoder.theengs.io/devices/H5174.html), [H5177](https://decoder.theengs.io/devices/H5177.html), [H5179](https://decoder.theengs.io/devices/H5179.html)
- **Inkbird**: [IBS-P01B](https://decoder.theengs.io/devices/IBS-P01B.html), [IBS-TH1](https://decoder.theengs.io/devices/IBS_TH1.html), [IBS-TH2](https://decoder.theengs.io/devices/IBS_TH2.html), [IBT_2X(S)](https://decoder.theengs.io/devices/IBT_2X.html), [IBT_4X(S/C)](https://decoder.theengs.io/devices/IBT_4XS.html), [IBT_6X(S)](https://decoder.theengs.io/devices/IBT_6XS.html), [ITH-12S](https://decoder.theengs.io/devices/ITH_12S.html)
- **Jaalee**: [F525/F51C](https://decoder.theengs.io/devices/JAALEE.html)
- **MOKOSMART**: [MBXPRO](https://decoder.theengs.io/devices/MBXPRO.html)
- **Oria/Brifit/SigmaWit/SensorPro**: [T201](https://decoder.theengs.io/devices/T201.html), [T301](https://decoder.theengs.io/devices/T301.html)
- **Qingping**: [CGDK2](https://decoder.theengs.io/devices/CGDK2.html)
- **rbaron**: [BPv1.0-1.2](https://decoder.theengs.io/devices/BPARASITE.html)
- **Sensirion**: [SCD4X](https://decoder.theengs.io/devices/SCD4X.html), [SHT4X](https://decoder.theengs.io/devices/SHT4x.html)
- **SensorBlue/Oria/Brifit**: [WS02/WS08](https://decoder.theengs.io/devices/ThermoBeacon.html)
- **Shelly**: [SBHT-003C](https://decoder.theengs.io/devices/SBHT-003C.html)
- **Ternergy**: [SOLIS_6](https://decoder.theengs.io/devices/SOLIS_6.html)
- **ThermoPro**: [TP357](https://decoder.theengs.io/devices/TP357.html), [TP358](https://decoder.theengs.io/devices/TP358.html), [TP359](https://decoder.theengs.io/devices/TP359.html), [TP393](https://decoder.theengs.io/devices/TP393.html)
- **Xiaomi**: [HHCCJCY10](https://decoder.theengs.io/devices/HHCCJCY10.html), [HHCCPOT002](https://decoder.theengs.io/devices/HHCCPOT002.html), [LYWSD03MMC_ATC/PVVX](https://decoder.theengs.io/devices/LYWSD03MMC.html), [LYWSDCGQ](https://decoder.theengs.io/devices/LYWSDCGQ.html), [MHO/MMC-C401_ATC/PVVX](https://decoder.theengs.io/devices/MHO-C401.html), [MJWSD05MMC_ATC/PVVX](https://decoder.theengs.io/devices/MJWSD05MMC.html)
- **Xiaomi/Mijia**: [LYWSD02](https://decoder.theengs.io/devices/LYWSD02.html)
- **Xiaomi/VegTrug**: [HHCCJCY01HHCC](https://decoder.theengs.io/devices/HHCCJCY01HHCC.html)

For a complete list, view the [list of  Theengs-compatible devices](https://decoder.theengs.io/devices/devices.html)
