#include "./http.h"
#include "./index_html.h"
#include "./src/fritz/fritz_session.h"
#include "./src/fritz/fritz_thermostat.h"
#include "./src/models/sensor.h"
#include "./src/bluetooth/SensorsScanner.h"
#include "./src/bluetooth/GoveeSensorsScanner.h"
#include "./src/bluetooth/TheengsSensorsScanner.h"

#include <EEPROM.h>
#include <esp_http_server.h> //TODO
#include <HTTPClient.h>
#include <NTPClient.h>
#include <string>
#include <vector>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <sstream>
#include <time.h>

// With this macro the board will try to read a wifi ssid and a pass from a camera if they're not specified.
// This option defined only when built with Platformio.
#ifdef READ_WIFI_QR_CODE
#include "./src/qrcode/qr_code_reader.h"
#endif

// ledPin refers to ESP32-CAM GPIO 4 (flashlight)
#define FLASH_GPIO_NUM 4
#define MAX_SENSORS_COUNT 20
#define MAX_ID_LENGTH 18
#define NTP_SERVER "europe.pool.ntp.org"
#define DEFAULT_FRITZBOX_HOST "fritz.box"
#define DEVICE_HOSTNAME "fritzgate"

//-------------------------------------------------------------------------------------
// Put your router settings here
//-------------------------------------------------------------------------------------
struct Config
{
  uint16_t signature = 0xF6F6;

  // Wifi network name (SSID)
  char wifi_ssid[32] = {};

  // Wifi network password
  char wifi_pass[64] = {};

  // IP address of your router. This should be "192.168.179.1" for most FRITZ!Boxes
  char fritz_host[32] = DEFAULT_FRITZBOX_HOST;

  // The username if you created an account, "admin" otherwise
  char fritz_user[32] = {};

  // The password for the fritzbox account.
  char fritz_pass[32] = {};

  // sensors bindings
  char sensors_bindings[MAX_SENSORS_COUNT][2][MAX_ID_LENGTH] = {};
};

Config defaultConfig;
Config config = {};

// json with fritzbox smart home control settings
unsigned long fritzSmartHomeControlLastRefreshMillis = 0;

char buffer[5 * 4 * 2 * 60 * 24 / 5] = {};

static int sensorsCount = 0;
static Sensor *sensors[MAX_SENSORS_COUNT] = {};
std::vector<Thermostat> thermostats;

const char FRITZ_STATUS_CONFIGURED[] = "CONFIGURED";
const char FRITZ_STATUS_NOT_CONFIGURED[] = "NOT_CONFIGURED";
const char FRITZ_STATUS_CONNECTION[] = "CONNECTING";
const char FRITZ_STATUS_CONNECTED[] = "CONNECTED";
const char FRITZ_STATUS_FAILURE[] = "FAILURE";
const char *fritzStatus = FRITZ_STATUS_NOT_CONFIGURED;

WiFiUDP ntpUDP;
Session fritzSession(DEFAULT_FRITZBOX_HOST, 80, "", "");
WiFiServer server(80);
NTPClient ntpClient(ntpUDP, NTP_SERVER, 3600, 60000);

Sensor *GetSensorById(const char *id)
{
  if (id == NULL || strlen(id) == 0)
    return NULL;
  for (int i = 0; i < sensorsCount && id != NULL; i++)
  {
    if (sensors[i]->id == id)
    {
      return sensors[i];
    }
  }
  return NULL;
}

void enableLed(bool state)
{
  digitalWrite(FLASH_GPIO_NUM, state ? HIGH : LOW);
}

const char *getThermostatBindingSensorId(const char *thermistatId)
{
  if (thermistatId == NULL || strlen(thermistatId) == 0)
    return "";
  for (int i = 0; i < MAX_SENSORS_COUNT && config.sensors_bindings[i][0][0] != 0; i++)
  {
    if (strcmp(config.sensors_bindings[i][0], thermistatId) == 0)
      return config.sensors_bindings[i][1];
  }
  return "";
}

Thermostat *getThermostatById(const char *thermostatId)
{
  if (thermostatId == NULL)
    return NULL;
  auto therm = std::find_if(
      thermostats.begin(), thermostats.end(),
      [&thermostatId](const Thermostat &x)
      { return x.id == thermostatId; });
  if (therm != thermostats.end())
    return &(*therm);
  return NULL;
}

Sensor *getThermostatBindingSensor(const char *thermistatId)
{
  const char *sensorId = getThermostatBindingSensorId(thermistatId);
  return GetSensorById(sensorId);
}

void updateThermostatBinding(const char *thermostatId, const char *sensorId)
{
  auto thermostat = getThermostatById(thermostatId);
  if (thermostat == NULL)
    return;

  thermostat->referenceSensor = sensorId;

  bool emptySensor = (sensorId == NULL || strlen(sensorId) == 0);
  for (int src = 0, dst = 0; src < MAX_SENSORS_COUNT; src++, dst++)
  {
    const char *curThermostatId = config.sensors_bindings[src][0];
    const char *curSensorId = config.sensors_bindings[src][1];
    if (strcmp(curThermostatId, thermostatId) == 0 || strlen(curThermostatId) == 0)
    {
      if (emptySensor && strcmp(curThermostatId, thermostatId) == 0)
      {
        dst--;
        continue;
      }
      curThermostatId = emptySensor ? "" : thermostatId;
      curSensorId = emptySensor ? "" : sensorId;
    }

    strncpy(config.sensors_bindings[dst][0], curThermostatId, MAX_ID_LENGTH);
    strncpy(config.sensors_bindings[dst][1], curSensorId, MAX_ID_LENGTH);
    if (strcmp(curThermostatId, thermostatId) == 0)
      break;
  }
}

/**
 * Scan for BLE servers and filter messages from themperature sensors.
 */
class SensorsAdvertisedDeviceCallbacks : public SensorScannerCallback
{
  /**
   * Called for each advertising BLE server.
   */
  void onResult(Sensor *sensorData)
  {
    long timestamp = ntpClient.getEpochTime();

    Sensor *sensor = GetSensorById(sensorData->id.c_str());

    long prevTimestamp;
    double prevTemperatureAvg;

    if (!sensor)
    {
      if (sensorsCount >= MAX_SENSORS_COUNT)
        return;
      sensor = sensors[sensorsCount++] = new Sensor();

      prevTimestamp = 0;
      prevTemperatureAvg = 0;
    }
    else
    {
      prevTimestamp = sensor->timestamp;
      prevTemperatureAvg = sensor->temperatureAvg;
    }

    sensor->temperature = sensorData->temperature;
    sensor->humidity = sensorData->humidity;
    sensor->model = sensorData->model;
    sensor->serial = sensorData->serial;
    sensor->name = sensorData->name;
    sensor->id = sensorData->id;
    sensor->rssi = sensorData->rssi;
    sensor->battery = sensorData->battery;
    sensor->timestamp = timestamp;
    sensor->data = sensorData->data;

    // calculate exponential moving average for temperature
    long interval = 5 * 60; // interval in seconds
    double alpha = min(interval, abs(timestamp - prevTimestamp)) / (double)interval;
    sensor->temperatureAvg = alpha * sensorData->temperature / 10.0 + (1 - alpha) * prevTemperatureAvg;
  }
};

/**
 * Get state of FritzBox thermostats and update temperature offset if required.
 */
bool syncFritzBox()
{
  fritzStatus = FRITZ_STATUS_CONNECTION;
  if (fritzSession.ensure_connection())
  {
    fritzStatus = FRITZ_STATUS_CONNECTED;
    bool updates = false;
    fritzSession.query_devices(thermostats);
    auto timestamp = ntpClient.getEpochTime();
    for (auto iter = thermostats.begin(); iter != thermostats.end(); iter++)
    {
      iter->timestamp = timestamp;
      iter->referenceSensor = getThermostatBindingSensorId(iter->id.c_str());
      Sensor *referenceSensor = GetSensorById(iter->referenceSensor.c_str());
      if (referenceSensor)
      {
        //  auto refTemperature = (int)iter->referenceSensor->temperature; // use realtime temp
        auto refTemperature = (int)(referenceSensor->temperatureAvg * 10); // use average temp
        auto offset = refTemperature - (int)iter->temperature;
        Serial.printf("Offset for %s: %g\r\n", iter->name.c_str(), offset / 10.0);
        // round(offset / 5.0)
        offset = ((offset + ((offset > 0) - (offset < 0)) * 5 / 2) / 5) * 5; // round to 0.5 grad
        offset = min(50, max(-50, offset));                                  // not more than 5 grad difference with ref sensor

        // update offset if it is changed and reference temperature not older than 5 minutes
        if (offset != iter->offset && timestamp - referenceSensor->timestamp < 60 * 5)
        {
          Serial.printf("Update offset for \"%s\" from %g to %g\r\n",
                        iter->name.c_str(),
                        iter->offset / 10.0,
                        offset / 10.0);
          iter->offset = offset;
          iter->offsetTimestamp = ntpClient.getEpochTime();

          FritzThermostat::printPostBody(Serial, *iter, fritzSession.sid().c_str());

          fritzSession.post_thermostat_state(*iter);
          updates = true;
        }
      }
    }

    if (updates)
    {
      bool result = fritzSession.query_devices(thermostats);
      auto timestamp = ntpClient.getEpochTime();
      for (auto iter = thermostats.begin(); iter != thermostats.end(); iter++)
      {
        iter->timestamp = timestamp;
        iter->referenceSensor = getThermostatBindingSensorId(iter->id.c_str());
      }
    }
    return true;
  }
  else
  {
    fritzStatus = FRITZ_STATUS_FAILURE;
  }
  return false;
}

/**
 * Setup WiFi connection.
 */
void setupWIFI()
{
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(config.wifi_ssid, config.wifi_pass);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  Serial.printf("Connecting to '%s'", config.wifi_ssid);
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++)
  {
    enableLed(i % 2);
    delay(500);
    Serial.print(".");
  }
  enableLed(false);
  Serial.printf("\r\nWifi connected, available at 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("'");
}

/**
 * Setup HTTP server.
 */
void setupHTTPD()
{
  server.begin();
}

/**
 * Setup BLE to listen for advertsing messages.
 */
void setupBLE()
{
  // SensorsScanner *pBLEScan = GoveeSensorsScanner::getScan(); // legacy scanner for Govee sensors
  SensorsScanner *pBLEScan = TheengsSensorsScanner::getScan();
  pBLEScan->init(DEVICE_HOSTNAME);

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.
  pBLEScan->start(new SensorsAdvertisedDeviceCallbacks());
}

/**
 * Setup NTP client.
 */
void setupNTP()
{
  ntpClient.begin();
  ntpClient.update();
}

EEPROMClass storage("eeprom0");

/**
 * Save configuration to EEPROM.
 */
bool saveConfig()
{
  storage.put(0, config);
  return storage.commit();
}

/**
 * Use a default configuration value if it is not defined.
 */
template <int n>
bool useDefaultIfNotDefined(
    char (&userValue)[n],
    const char (&defaultValue)[n],
    bool signatureMismatch,
    bool defaultPriority)
{
  if (signatureMismatch || (defaultPriority && strlen(defaultValue) > 0) || (!defaultPriority && strlen(userValue) == 0))
  {
    strncpy(userValue, defaultValue, n);
    return true;
  }
  return false;
}

/**
 * Load configuration from EEPROM.
 */
void setupConfig()
{
  while (!storage.begin(sizeof(Config)))
  {
    Serial.println("Failed to initialise EEPROM");
  }

  storage.get(0, config);

  bool signatureMismatch = config.signature != defaultConfig.signature;
  if (signatureMismatch)
  {
    Serial.printf("Signature mismatch: %x whjile expected %x\r\n", config.signature, defaultConfig.signature);
  }

  config.signature = defaultConfig.signature;

  // use default value for wifi settings if it defined in code as user cant change it
  useDefaultIfNotDefined(config.wifi_ssid, defaultConfig.wifi_ssid, signatureMismatch, true);
  useDefaultIfNotDefined(config.wifi_pass, defaultConfig.wifi_pass, signatureMismatch, true);
  // for the rest params use default value only if it is changed by user
  useDefaultIfNotDefined(config.fritz_user, defaultConfig.fritz_user, signatureMismatch, false);
  useDefaultIfNotDefined(config.fritz_pass, defaultConfig.fritz_pass, signatureMismatch, false);
  useDefaultIfNotDefined(config.fritz_host, defaultConfig.fritz_host, signatureMismatch, false);

  if (signatureMismatch || (defaultConfig.sensors_bindings[0][0][0] != 0))
  {
    Serial.println("Reset sensors bindings");
    memcpy(config.sensors_bindings, defaultConfig.sensors_bindings, sizeof(defaultConfig.sensors_bindings));
  }

  saveConfig();
}

/**
 * Setup Arduino application.
 */
void setup()
{
  Serial.begin(115200);
  Serial.println("Starting Arduino application...");

  setupConfig();

#ifdef READ_WIFI_QR_CODE
  if (config.wifi_ssid == NULL || strlen(config.wifi_ssid) == 0 ||
      config.wifi_pass == NULL || strlen(config.wifi_pass) == 0)
  {
    std::string wifiCredentials = ReadWifiQrCode();
    std::string key, value;
    std::istringstream stream(wifiCredentials);
    if (getline(stream, key, ':') && key == "WIFI")
    {
      while (getline(stream, key, ':') && getline(stream, value, ';'))
      {
        if (key == "S")
          strncpy(config.wifi_ssid, value.c_str(), sizeof(config.wifi_ssid));
        else if (key == "P")
          strncpy(config.wifi_pass, value.c_str(), sizeof(config.wifi_pass));
      }
      // save configuration with defined ssid and pass
      saveConfig();
    }
    // reboot
    ESP.restart();
  }
#endif

  // initialize digital pin ledPin as an output
  pinMode(FLASH_GPIO_NUM, OUTPUT);

  if (strlen(config.fritz_host) > 0 &&
      strlen(config.fritz_user) > 0 &&
      strlen(config.fritz_pass) > 0)
    fritzStatus = FRITZ_STATUS_CONFIGURED;

  fritzSession.configure(config.fritz_host, 80, config.fritz_user, config.fritz_pass);
  setupWIFI();
  setupBLE();
  setupHTTPD();
  setupNTP();
}

/**
 * Checks whether string starts with specified prefix.
 */
bool startsWith(const char *str, const char *prefix)
{
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

/**
 * This is the Arduino main loop function.
 */
void loop()
{
  // Fetch data from FritzBox every 30 seconds
  if (
      strlen(config.fritz_host) > 0 &&
      strlen(config.fritz_user) > 0 &&
      strlen(config.fritz_pass) > 0 &&
      millis() - fritzSmartHomeControlLastRefreshMillis >= 1 * 30 * 1000UL)
  {
    fritzSmartHomeControlLastRefreshMillis = millis();
    syncFritzBox();
  }

  // listen for incoming clients
  WiFiClient client = server.available();

  if (client)
  {
    // an http request ends with a blank line
    bool currentLineIsBlank = true;
    char linebuf[64];
    int lineIndex = 0;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        if (lineIndex < sizeof(linebuf) && (lineIndex == 0 || linebuf[lineIndex - 1] != '\n'))
          linebuf[lineIndex++] = c;
        //  if you've gotten to the end of the line (received a newline
        //  character) and the line is blank, the http request has ended,
        //  so you can send a reply
        if (c == '\n' && currentLineIsBlank)
        {
          linebuf[lineIndex - 1] = 0;
          Serial.println(linebuf);

          if (startsWith(linebuf, "GET /api/status"))
          {
            long timestamp = ntpClient.getEpochTime();
            writeJsonHeaders(client);
            client.printf("{\"fritz\":{\"status\":\"%s\",\"host\":\"%s\",\"user\":\"%s\"},\"sensors\":[", fritzStatus, config.fritz_host, config.fritz_user);
            for (int i = 0; i < sensorsCount; i++)
            {
              sensors[i]->Serialize(client, timestamp, 1);
              client.print(i + 1 < sensorsCount ? "," : "");
            }
            client.println("], \"thermostats\":[");
            for (int i = 0; i < thermostats.size(); i++)
            {
              thermostats[i].Serialize(client, timestamp, 1);
              client.print(i + 1 < thermostats.size() ? "," : "");
            }
            client.println("\r\n]}");
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "OPTIONS /api/thermostat") || startsWith(linebuf, "OPTIONS /api/fritzauth"))
          {
            writeJsonHeaders(client);
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "POST /api/fritzauth"))
          {
            DynamicJsonDocument fritzauth(200);
            deserializeJson(fritzauth, client, DeserializationOption::NestingLimit(2));

            const char *host = fritzauth["host"].as<const char *>();
            const char *user = fritzauth["user"].as<const char *>();
            const char *pass = fritzauth["pass"].as<const char *>();

            if (host != NULL && strlen(host) > 0 && user != NULL && strlen(user) > 0 && pass != NULL && strlen(pass) > 0)
            {
              strncpy(config.fritz_host, host, sizeof(config.fritz_host));
              strncpy(config.fritz_user, user, sizeof(config.fritz_user));
              strncpy(config.fritz_pass, pass, sizeof(config.fritz_pass));
              fritzStatus = FRITZ_STATUS_CONFIGURED;
              saveConfig();
              fritzSession.configure(config.fritz_host, 80, config.fritz_user, config.fritz_pass);
            }

            writeJsonHeaders(client);
            client.print("{}");
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "POST /api/thermostat"))
          {
            DynamicJsonDocument thermostat(200);
            deserializeJson(thermostat, client, DeserializationOption::NestingLimit(2));

            const char *thermostatId = thermostat["id"].as<const char *>();
            const char *referenceSensor = thermostat["referenceSensor"].as<const char *>();
            updateThermostatBinding(thermostatId, referenceSensor);
            saveConfig();

            writeJsonHeaders(client);

            client.printf("{\"sensors_bindings\":[");
            for (int i = 0; i < MAX_SENSORS_COUNT && config.sensors_bindings[i][0][0] != 0; i++)
            {
              client.printf("[\"%s\",\"%s\"]%s",
                            config.sensors_bindings[i][0],
                            config.sensors_bindings[i][1],
                            (i == MAX_SENSORS_COUNT - 1 || config.sensors_bindings[i + 1][0][0]) == 0 ? "" : ",");
            }
            client.print("]}");
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "GET /api/") || startsWith(linebuf, "POST /api/") || startsWith(linebuf, "GET /favicon.ico"))
          {
            writeHttpStatus(client, 404, "Not found");
            client.println();
            client.flush();
            break;
          }
          else
          {
            writeHttpStatus(client);
            writeHttpHeader(client, HeaderContentLength, std::to_string(index_html_length).c_str());
            writeHttpHeader(client, HeaderContentType, "text/html; charset=UTF-8");
            writeHttpHeader(client, HeaderContentEncoding, "gzip");
            client.println();
            client.write_P((const char *)index_html, index_html_length);
            client.flush();
            break;
          }
        }

        if (c == '\n')
        {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r')
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }

    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
  }
} // End of loop