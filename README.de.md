# FritzGate

[![en](https://img.shields.io/badge/lang-en-blue.svg)](README.md)
[![de](https://img.shields.io/badge/lang-de-red.svg)](README.de.md)
[![PlatformIO CI](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/platformio.yml)
[![Frontend CI](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml/badge.svg)](https://github.com/elebree/fritzgate/actions/workflows/deploy.yml)

**FritzGate** verwandelt ein **Ai Thinker** Board in einen intelligenten Thermostat-Controller. Es arbeitet mit Thermostaten, die mit einer **FRITZ!Box** verbunden sind (`FRITZ!DECT 301` und `FRITZ!DECT 302`), und nutzt für eine präzise Regelung der Raumtemperatur einen von über 70 Bluetooth-Temperatursensoren. Unterstützt werden unter anderem Geräte von Govee, Xiaomi, Qingping, Ruuvi, Inkbird, SwitchBot und ThermoPro.

[![FritzGate screenshot](docs/FritzGate_Screenshot.svg)](https://fritzgate.elebree.com/demo/)

## Demo

Eine Online-Demo findest du [hier](https://fritzgate.elebree.com/demo/).

## Funktionen

- **FRITZ!Box-Integration:** FritzGate liest deine Thermostate direkt aus der FRITZ!Box. Es ändert nur den Temperatur-Offset und lässt Wochenpläne und alle anderen Thermostateinstellungen unverändert.
- **Externe Temperatursensoren:** Ein Thermostat misst die Temperatur am Heizkörper. Verknüpfe es mit einem Sensor im Raum, und FritzGate korrigiert laufend den Temperatur-Offset des Thermostats, damit der Raum die eingestellte Temperatur erreicht.
- **Eingebaute Weboberfläche:** Überwache Temperatur, Luftfeuchtigkeit, Batterie und Signal der Sensoren in der Nähe, und verwalte Thermostat-Sensor-Verknüpfungen im Browser.

## Vorteile

- **Einfache Konfiguration:** Richte das WLAN ein, indem du der Kamera des Boards einen WLAN-QR-Code zeigst.
- **Kostengünstige Hardware:** Das **ESP32-CAM** Board ist preiswert und energieeffizient.
- **Stromversorgung über die FRITZ!Box:** Versorge das Board über den USB-Anschluss der **FRITZ!Box** mit Strom. Du brauchst keinen zusätzlichen Platz und kein eigenes Netzteil.
- **Über 70 unterstützte Sensoren:** Verwende eines von [über 70 Modellen](https://decoder.theengs.io/devices/devices.html) von Temperatursensoren für angenehme Räume und weniger Energieverbrauch.
- **Auch ohne FRITZ!Box nutzbar:** Auch ohne Zugang zur FRITZ!Box kannst du Temperatur und Luftfeuchtigkeit der Sensoren in der Nähe überwachen.

## Funktionsweise

- Das Board empfängt die Bluetooth-Advertisements der Sensoren in der Nähe und berechnet für jeden Sensor eine geglättete Temperatur.
- Alle 30 Sekunden liest es die Thermostate aus der FRITZ!Box. Für jedes Thermostat, das mit einem Sensor verknüpft ist, berechnet es den Offset als *Sensortemperatur − Thermostattemperatur*, rundet ihn auf 0,5 °C, begrenzt ihn auf ±5 °C und sendet ihn an die FRITZ!Box, wenn er sich geändert hat.
- Der Offset bleibt unverändert, wenn der letzte Messwert des Sensors älter als 5 Minuten ist oder die Heizung am Thermostat ausgeschaltet ist.
- Wenn 5 Minuten lang weder die FRITZ!Box noch ein Sensor neue Daten liefert, startet das Board neu.

## Erste Schritte

### Hardware-Anforderungen

- Ai Thinker ESP32-CAM Board und ein USB-Seriell-Adapter (oder ein ESP32-CAM-MB Board) zum Flashen
- [Temperatursensoren](https://decoder.theengs.io/devices/devices.html)
- FRITZ!Box mit kompatiblen Thermostaten (optional)

### Installation

1. Klone das Repository auf deinen Computer:

    ```bash
    git clone https://github.com/elebree/fritzgate.git
    ```

2. Baue die Firmware und lade sie mit [PlatformIO](https://platformio.org/) (empfohlen) oder der [Arduino IDE](https://www.arduino.cc/en/software) hoch.

3. **PlatformIO:**
   - Lade die Firmware vom Hauptverzeichnis des Repositorys aus hoch:

     ```bash
     pio run -t upload
     ```

   - Zeige der Kamera des Boards beim ersten Start einen WLAN-QR-Code. Der Code muss das Standardformat `WIFI:S:<Netzwerk>;T:WPA;P:<Passwort>;;` verwenden. Die Benutzeroberfläche der FRITZ!Box und die meisten Smartphones können einen solchen Code für dein Netzwerk anzeigen. Das Board speichert die Zugangsdaten und startet neu. Während das Board sich mit dem WLAN verbindet, blinkt die Blitz-LED.

4. **Arduino IDE:**
   - Installiere das Boardpaket **esp32** in Version **2.0.x**. Der PlatformIO-Build verwendet 2.0.11. Version 3.x wird nicht unterstützt.
   - Installiere diese Bibliotheken: [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.x, [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 1.x, [TheengsDecoder](https://github.com/theengs/decoder) 1.7.8 und [NTPClient](https://github.com/arduino-libraries/NTPClient).
   - Öffne `src/src.ino` und wähle das Board **AI Thinker ESP32-CAM** mit dem Partitionsschema **Huge APP**. Die Firmware ist etwa 1,5 MB groß und passt nicht in die Standard-App-Partition.
   - Trage dein WLAN und dein Passwort in `wifi_ssid` und `wifi_pass` der Struktur `Config` in `src/src.ino` ein. Alternativ kannst du [ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader) installieren und `#define READ_WIFI_QR_CODE` am Anfang von `src/src.ino` einfügen, um den WLAN-QR-Code wie oben beschrieben zu lesen.
   - Lade den Sketch auf dein Board hoch.

5. Öffne die Weboberfläche unter [http://fritzgate](http://fritzgate) oder unter der IP-Adresse, die dein Router dem Board zugewiesen hat. Der serielle Monitor (115200 Baud) zeigt die Adresse ebenfalls an.

6. Öffne **Settings** und verbinde FritzGate mit deiner FRITZ!Box. Gib den Host (meist `fritz.box`) sowie Benutzername und Passwort eines FRITZ!Box-Benutzers mit der Berechtigung „Smart Home“ ein.

7. Wähle auf der Seite **Settings** für jedes Thermostat einen Referenzsensor aus.

## Kompatible Sensoren

FritzGate funktioniert mit jedem Gerät, das [TheengsDecoder](https://github.com/theengs/decoder) v1.7.8 dekodieren kann und das eine Temperatur meldet. Einige Beispiele:

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

Die [Liste der Theengs-kompatiblen Geräte](https://decoder.theengs.io/devices/devices.html) folgt der neuesten Version von TheengsDecoder. Sie kann neuere Modelle enthalten, zum Beispiel den Shelly BLU H&T oder den Govee H5108, die FritzGate noch nicht unterstützt.

## Entwicklung

Das Repository besteht aus zwei Teilen, die zusammen als ein Firmware-Image ausgeliefert werden:

- `src/` ist die ESP32-Firmware (C++, Arduino-Framework, gebaut mit PlatformIO).
- `frontend/` ist die Svelte-Weboberfläche. Sie wird als `src/index_html.h` in die Firmware einkompiliert.

Firmware-Befehle, ausgeführt im Hauptverzeichnis des Repositorys:

```bash
pio run                 # bauen
pio run -t upload       # bauen und flashen
pio device monitor      # serieller Monitor, 115200 Baud
```

Wenn du die Weboberfläche änderst, führe in `frontend/` den Befehl `pnpm run build:firmware` aus, um `src/index_html.h` neu zu erzeugen. Baue und flashe danach die Firmware erneut. Details findest du in [frontend/README.md](frontend/README.md) (auf Englisch).

## Lizenz

FritzGate steht unter der [MIT-Lizenz](LICENSE).
