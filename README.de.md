# FritzGate

[![en](https://img.shields.io/badge/lang-en-blue.svg)](README.md)
[![de](https://img.shields.io/badge/lang-de-red.svg)](README.de.md)
[![PlatformIO CI](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml)
[![Frontend CI](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml)

FritzGate verwandelt Ihr **Ai Thinker Board** in einen intelligenten Thermostat-Controller. Dieses Projekt ermöglicht die nahtlose Steuerung von Thermostaten, die mit einer **FRITZ!Box** (FRITZ!DECT 301 und FRITZ!DECT 302) verbunden sind, und integriert über 70 Temperaturfühler von führenden Herstellern wie Govee, Xiaomi, Shelly, Ruuvi, Inkbird, SwitchBot und ThermoPro für eine präzise Temperaturregelung. Eine vollständige Liste finden Sie in der [Liste der Theengs-kompatiblen Geräte](https://decoder.theengs.io/devices/devices.html).

[![FritzGate screenshot](docs/FritzGate_Screenshot.svg)](https://elebree.github.io/fritzgate/)

## Demo

Eine Demo der FritzGate-Oberfläche ist [hier](https://elebree.github.io/fritzgate/) verfügbar.

## Funktionen

- **FRITZ!Box-Integration:** FritzGate beeinträchtigt keine Wochenpläne oder andere Thermostateinstellungen. Verbinden und steuern Sie Thermostate direkt über die FRITZ!Box.
- **Verknüpfung mit externen Temperatursensoren:** Nutzen Sie externe Temperatursensoren, die in Wohnräumen platziert sind, für eine präzise Temperaturregelung in einem Raum. FritzGate korrigiert automatisch Temperaturabweichungen der Thermostat-Sensoren.
- **Eingebauter Webserver:** Verwalten Sie Thermostat-Sensor-Verknüpfungen und überwachen Sie Temperatur und Luftfeuchtigkeit über eine benutzerfreundliche Weboberfläche.

## Vorteile

- **Einfache Konfiguration:** Einrichtung durch einfaches Scannen eines QR-Codes mit der Kamera des Ai Thinker Boards für die WLAN-Zugangsdaten.
- **Kostengünstige Hardware:** Nutzen Sie das preiswerte und energieeffiziente **ESP32-CAM** Board.
- **Stromversorgung über FRITZ!Box:** Profitieren Sie von der Möglichkeit, das Gerät direkt über den USB-Anschluss der FRITZ!Box mit Strom zu versorgen, ohne zusätzlichen Platz oder weitere Stromquellen zu benötigen.
- **Unterstützung von über 70 Sensoren:** Verwenden Sie [70+ Modelle](https://decoder.theengs.io/devices/devices.html) von Temperatursensoren für eine präzise Klimaregelung in Wohnräumen, um optimalen Komfort und Energieeinsparungen zu gewährleisten.
- **Eingebauter Webserver:** Verwalten Sie Thermostat-Sensor-Verknüpfungen über den integrierten Webserver und bieten Sie den Nutzern eine intuitive Schnittstelle, um ihr intelligentes Heizsystem anzupassen und zu überwachen.
- **Kann ohne FRITZ!Box verwendet werden:** Auch ohne Berechtigung für die FRITZ!Box können Sie Temperatur und Luftfeuchtigkeit von nahegelegenen Temperatursensoren überwachen.

## Erste Schritte

### Hardware-Anforderungen

- Ai Thinker ESP32-CAM Board
- [Temperatursensoren](https://decoder.theengs.io/devices/devices.html)
- FRITZ!Box mit kompatiblen Thermostaten (optional)

### Installation

1. Klonen Sie das Repository auf Ihren lokalen Computer:

    ```bash
    git clone https://github.com/elebree/fritzgate.git
    ```

2. Öffnen Sie das Projekt in [PlatformIO](https://platformio.org/) oder der [Arduino IDE](https://www.arduino.cc/en/software), je nach Vorliebe.

3. **Für PlatformIO-Benutzer:**
   - Laden Sie den Code auf Ihr Ai Thinker ESP32-CAM Board hoch.
   - Nach dem ersten Start können Sie mit der Kamera des Ai Thinker ESP32-CAM Boards den WLAN-QR-Code scannen. Dadurch werden die WLAN-Zugangsdaten automatisch konfiguriert.

4. **Für Arduino IDE-Benutzer:**
   - Stellen Sie sicher, dass Sie die [NTPClient](https://github.com/arduino-libraries/NTPClient) und [ArduinoJson](https://github.com/bblanchon/ArduinoJson) Arduino-Bibliotheken installiert haben, bevor Sie das Projekt kompilieren.
   - Geben Sie die WLAN-Zugangsdaten direkt im Code ein (alternativ können Sie die [ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader) Bibliothek von GitHub installieren, um den WLAN-QR-Code zu lesen).
   - Laden Sie den Code auf Ihr Ai Thinker ESP32-CAM Board hoch.

5. Greifen Sie auf die Weboberfläche zu, indem Sie die zugewiesene IP-Adresse in Ihrem Webbrowser eingeben.
