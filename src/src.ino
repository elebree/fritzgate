#include "./http.h"
#include "./index_html.h"
#include "./src/fritz/fritz_session.h"
#include "./src/fritz/fritz_thermostat.h"
#include "./src/models/sensor.h"
#include "./src/bluetooth/SensorsScanner.h"
#include "./src/bluetooth/GoveeSensorsScanner.h"
#include "./src/bluetooth/TheengsSensorsScanner.h"
#include "./src/history/history.h"
#include "./src/logging.h"
#include "./src/json.h"
#include "./src/buffered_print.h"

#include <EEPROM.h>
#include <esp_http_server.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <string>
#include <vector>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <sstream>
#include <time.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_timer.h>

// GPIO for ESP32-CAM flash (LED), typically GPIO 4
#define FLASH_GPIO_NUM 4

// If this macro is defined, the board will try to read WiFi SSID and password from a QR code using camera if not specified manually.
// This is enabled only when built with PlatformIO.
#ifdef READ_WIFI_QR_CODE
#include "./src/qrcode/qr_code_reader.h"
#endif

// Release tag of the firmware, such as "v1.2.0". release.yml passes it as a build flag (-D FRITZGATE_VERSION=\"v1.2.0\"
// in PLATFORMIO_BUILD_FLAGS); other builds are "dev". Logged at start and reported in /api/status.
#ifndef FRITZGATE_VERSION
#define FRITZGATE_VERSION "dev"
#endif

// Watchdog timer: the device restarts automatically if FRITZ!Box credentials are set, but the FRITZ!Box doesn't answer
// the list of thermostats for 5 minutes. Without credentials, loop() feeds it.
#define WATCHDOG_TIMEOUT 60 * 5

// FritzBox settings refresh interval: 30 seconds
#define FRITZGATE_REFRESH_TIMEOUT 30

// Seconds a reference sensor reading is used to correct the offset: 5 minutes
#define REFERENCE_STALE_TIMEOUT (60 * 5)

// Seconds without a reference sensor reading after which the thermostat gets back the offset it had before it was
// bound, until the sensor reports again: 1 hour
#define REFERENCE_LOST_TIMEOUT (60 * 60)

// Seconds between NTP clock syncs: 1 hour. Between syncs, NTPClient counts the time with millis(), which wraps after
// 49.7 days and would put the clock back by as much.
#define NTP_SYNC_INTERVAL (60 * 60)

// Seconds between NTP attempts while the clock was never synced: 1 minute. A failed attempt blocks loop() for a second.
#define NTP_RETRY_INTERVAL 60

// Seconds the clock is ahead of UTC: 1 hour, CET without daylight saving time. The timestamps in /api/status use this
// clock, /api/history uses UTC.
#define NTP_TIME_OFFSET 3600

// Seconds between history samples of the sensors and thermostats: 1 minute
#define HISTORY_SAMPLE_INTERVAL 60

// sh_control reads that may be tried to migrate the thermostat bindings to UIDs after a start
#define BINDINGS_MIGRATION_ATTEMPTS 3

// Marks Config::sensors_bindings_offsets as initialized
#define OFFSETS_SIGNATURE 0xA501

// Maximum number of sensors that can be handled
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

  // IP address of your router. For most FRITZ!Boxes, this is "192.168.178.1"
  char fritz_host[32] = DEFAULT_FRITZBOX_HOST;

  // Username for FritzBox account. Default is "admin" if no custom account was created.
  char fritz_user[32] = {};

  // Password for the FritzBox account
  char fritz_pass[32] = {};

  // Sensor bindings (thermostat-sensor pairings), filled from the start. A thermostat with an empty sensor id was
  // unbound: it keeps its binding until syncFritzBox() has restored its original offset.
  char sensors_bindings[MAX_SENSORS_COUNT][2][MAX_ID_LENGTH] = {};

  // Fields added later go below, so that the fields above keep their place in a saved config. The EEPROM library fills
  // the bytes it adds to a saved config with 0xFF.

  // Offset in 0.1 °C that each bound thermostat had before it was bound, by binding index. It is restored when the
  // thermostat is unbound or its reference sensor is lost.
  int8_t sensors_bindings_offsets[MAX_SENSORS_COUNT] = {};

  // OFFSETS_SIGNATURE once sensors_bindings_offsets is initialized
  uint16_t offsets_signature = OFFSETS_SIGNATURE;
};

// Default and active configurations
Config defaultConfig;
Config config = {};

// Tracks the last refresh timestamp for FritzBox Smart Home control
unsigned long fritzSmartHomeControlLastRefreshMillis = 0;

// Tracks when loop() last fed the watchdog while no FRITZ!Box credentials are set
unsigned long watchdogLastFeedMillis = 0;

// Tracks the last NTP clock sync attempt
unsigned long ntpLastSyncMillis = 0;

// Tracks the last history sample
unsigned long historyLastSampleMillis = 0;

// char buffer[5 * 4 * 2 * 60 * 24 / 5] = {}; // overflowed by 1136 bytes

// Holds pointers to active sensors
static int sensorsCount = 0;
static Sensor *sensors[MAX_SENSORS_COUNT] = {};
std::vector<Thermostat> thermostats;

// Readings of the last 30 days, served by /api/history
History history;

// FritzBox connection states
const char FRITZ_STATUS_CONFIGURED[] = "CONFIGURED";
const char FRITZ_STATUS_NOT_CONFIGURED[] = "NOT_CONFIGURED";
const char FRITZ_STATUS_CONNECTION[] = "CONNECTING";
const char FRITZ_STATUS_CONNECTED[] = "CONNECTED";
const char FRITZ_STATUS_FAILURE[] = "FAILURE";
const char *fritzStatus = FRITZ_STATUS_NOT_CONFIGURED;

WiFiUDP ntpUDP;
Session fritzSession(DEFAULT_FRITZBOX_HOST, 80, "", "");
WiFiServer server(80);
NTPClient ntpClient(ntpUDP, NTP_SERVER, NTP_TIME_OFFSET, 60000);

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

/**
 * Whether a string fits a fixed-size field with its terminating zero.
 */
template <int n>
bool fitsField(const char (&field)[n], const char *value)
{
  return value != NULL && strlen(value) < n;
}

/**
 * Copy a string into a fixed-size field. Returns false, and leaves the field unchanged, if it doesn't fit.
 */
template <int n>
bool copyField(char (&field)[n], const char *value)
{
  if (!fitsField(field, value))
    return false;
  memcpy(field, value, strlen(value) + 1);
  return true;
}

/**
 * Terminate a fixed-size field. Older versions could fill it without its terminating zero.
 */
template <int n>
void terminateField(char (&field)[n])
{
  field[n - 1] = 0;
}

/**
 * Number of thermostat bindings, which fill config.sensors_bindings from the start.
 */
int countThermostatBindings()
{
  int count = 0;
  while (count < MAX_SENSORS_COUNT && config.sensors_bindings[count][0][0] != 0)
    count++;
  return count;
}

/**
 * Index of the binding of a thermostat in config.sensors_bindings, or -1.
 */
int findThermostatBinding(const char *thermostatId)
{
  if (thermostatId == NULL || thermostatId[0] == 0)
    return -1;
  int count = countThermostatBindings();
  for (int i = 0; i < count; i++)
  {
    if (strcmp(config.sensors_bindings[i][0], thermostatId) == 0)
      return i;
  }
  return -1;
}

/**
 * Remove a thermostat binding, and move the bindings after it up.
 */
void removeThermostatBinding(int index)
{
  int count = countThermostatBindings();
  if (index < 0 || index >= count)
    return;
  for (int i = index; i + 1 < count; i++)
  {
    memcpy(config.sensors_bindings[i], config.sensors_bindings[i + 1], sizeof(config.sensors_bindings[i]));
    config.sensors_bindings_offsets[i] = config.sensors_bindings_offsets[i + 1];
  }
  memset(config.sensors_bindings[count - 1], 0, sizeof(config.sensors_bindings[count - 1]));
  config.sensors_bindings_offsets[count - 1] = 0;
}

const char *getThermostatBindingSensorId(const char *thermostatId)
{
  int index = findThermostatBinding(thermostatId);
  return index < 0 ? "" : config.sensors_bindings[index][1];
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

/**
 * Set the reference sensor of a thermostat; an empty sensor id unbinds it. A new binding saves the thermostat's
 * current offset, and an unbound thermostat keeps its binding until syncFritzBox() has restored that offset.
 * Returns false if the thermostat is unknown, an id is too long or all bindings are in use.
 */
bool updateThermostatBinding(const char *thermostatId, const char *sensorId)
{
  auto thermostat = getThermostatById(thermostatId);
  if (thermostat == NULL || !fitsField(config.sensors_bindings[0][1], sensorId))
    return false;

  int index = findThermostatBinding(thermostatId);
  if (index < 0)
  {
    if (sensorId[0] == 0)
    {
      // not bound, nothing to restore
      thermostat->referenceSensor = "";
      return true;
    }
    index = countThermostatBindings();
    if (index >= MAX_SENSORS_COUNT || !copyField(config.sensors_bindings[index][0], thermostatId))
      return false;
    config.sensors_bindings_offsets[index] = (int8_t)min(127, max(-127, (int)thermostat->offset));
  }
  copyField(config.sensors_bindings[index][1], sensorId);
  thermostat->referenceSensor = sensorId;
  return true;
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
 * Write the offset of a thermostat, unless its state or the FRITZ!Box doesn't allow it now.
 * Returns true if the FRITZ!Box took the offset.
 */
bool writeThermostatOffset(Thermostat &thermostat, int32_t offset)
{
  if (thermostat.targetTemperature == 0)
  {
    LOG_WARNINGF("Skip offset for \"%s\" %g -> %g: heating off",
                 thermostat.name.c_str(),
                 thermostat.offset / 10.0,
                 offset / 10.0);
    return false;
  }
  if (!thermostat.temperatureValid)
  {
    LOG_WARNINGF("Skip offset for \"%s\": FRITZ!Box reports no temperature", thermostat.name.c_str());
    return false;
  }

  ThermostatApi *api = fritzSession.api();
  if (api == NULL)
  {
    // the session ended during this sync
    LOG_WARNINGF("Skip offset for \"%s\" %g -> %g: not logged in",
                 thermostat.name.c_str(),
                 thermostat.offset / 10.0,
                 offset / 10.0);
    return false;
  }
  if (!api->setOffset(thermostat, offset))
    return false;
  thermostat.offset = offset;
  thermostat.offsetTimestamp = ntpClient.getEpochTime();
  return true;
}

/**
 * Correct the offset of a bound thermostat from its reference sensor. When the thermostat was unbound or its sensor
 * is lost, give it back the offset it had before it was bound. Returns true if an offset was sent to the FRITZ!Box.
 */
bool correctThermostat(Thermostat &thermostat, int binding, unsigned long timestamp)
{
  const char *sensorId = config.sensors_bindings[binding][1];
  int32_t originalOffset = config.sensors_bindings_offsets[binding];
  bool externalSensor = thermostat.usesExternalSensor();

  if (sensorId[0] == 0)
  {
    // Unbound: once the original offset is back, forget the binding. An external sensor set in FRITZ!Box since then
    // means that the user took over, so leave the thermostat alone.
    if (thermostat.offset == originalOffset || externalSensor)
    {
      LOG_INFOF("\"%s\": unbound, offset %g", thermostat.name.c_str(), thermostat.offset / 10.0);
      removeThermostatBinding(binding);
      saveConfig();
      return false;
    }
    LOG_INFOF("\"%s\": unbound, restoring the offset it had before", thermostat.name.c_str());
    return writeThermostatOffset(thermostat, originalOffset);
  }

  Sensor *referenceSensor = GetSensorById(sensorId);
  // Signed, as the sensor can report while this sync runs. A reading from further in the future means that the clock
  // went back, so its real age is unknown: don't use it.
  long age = referenceSensor ? (long)(timestamp - referenceSensor->timestamp) : (long)(millis() / 1000);
  if (referenceSensor && age > -REFERENCE_STALE_TIMEOUT && age < REFERENCE_STALE_TIMEOUT)
  {
    auto refTemperature = (int)(referenceSensor->temperatureAvg * 10); // use average temp
    auto offset = refTemperature - (int)thermostat.temperature;
    offset = ((offset + ((offset > 0) - (offset < 0)) * 5 / 2) / 5) * 5; // round to 0.5 grad
    offset = min(50, max(-50, offset));                                  // not more than 5 grad difference with ref sensor

    // update offset if it is changed, or the thermostat still uses an external sensor set in FRITZ!Box
    if (offset != thermostat.offset || externalSensor)
      return writeThermostatOffset(thermostat, offset);
    return false;
  }

  // No recent reading, or none since the start: keep the offset for a while, then restore the original one until the
  // sensor reports again. A reading from the future keeps the offset until the sensor reports again. Leave an external
  // sensor set in FRITZ!Box alone, as above.
  if (age >= REFERENCE_LOST_TIMEOUT && thermostat.offset != originalOffset && !externalSensor)
  {
    LOG_WARNINGF("\"%s\": reference sensor %s not seen for %ld min, restoring the offset it had before",
                 thermostat.name.c_str(),
                 sensorId,
                 age / 60);
    return writeThermostatOffset(thermostat, originalOffset);
  }
  return false;
}

/**
 * Whether a thermostat binding has the numeric device id of sh_control, which the REST API doesn't report.
 */
bool hasNumericThermostatBindings()
{
  int count = countThermostatBindings();
  for (int i = 0; i < count; i++)
  {
    const char *thermostatId = config.sensors_bindings[i][0];
    if (thermostatId[strspn(thermostatId, "0123456789")] == 0)
      return true;
  }
  return false;
}

/**
 * Rename the bindings saved with the numeric device ids of sh_control to the unit UIDs, which the REST API uses as
 * thermostat ids. sh_control, still served by FRITZ!OS 8.2x, reports both. Runs once per start; a failed read is
 * retried with the next syncs, up to BINDINGS_MIGRATION_ATTEMPTS reads. A failed read can end the session, so that the
 * sync reads no thermostats and doesn't feed the watchdog: retrying forever would restart the board every 5 minutes.
 */
void migrateThermostatBindings()
{
  static int attempts = 0;
  if (attempts >= BINDINGS_MIGRATION_ATTEMPTS || !hasNumericThermostatBindings())
    return;

  attempts++;
  std::vector<Thermostat> legacyThermostats;
  if (!fritzSession.dataLuaApi().queryThermostats(legacyThermostats))
  {
    LOG_WARNINGF("Thermostat bindings not migrated to UIDs, %s",
                 attempts < BINDINGS_MIGRATION_ATTEMPTS ? "retrying with the next sync" : "bind the sensors again");
    return;
  }
  attempts = BINDINGS_MIGRATION_ATTEMPTS;

  bool changed = false;
  int count = countThermostatBindings();
  for (int i = 0; i < count; i++)
  {
    char *thermostatId = config.sensors_bindings[i][0];
    if (thermostatId[strspn(thermostatId, "0123456789")] != 0)
      continue;
    auto thermostat = std::find_if(
        legacyThermostats.begin(), legacyThermostats.end(),
        [thermostatId](const Thermostat &x)
        { return x.id == thermostatId && !x.uid.empty(); });
    if (thermostat == legacyThermostats.end() || findThermostatBinding(thermostat->uid.c_str()) >= 0 ||
        !fitsField(config.sensors_bindings[i][0], thermostat->uid.c_str()))
    {
      LOG_WARNINGF("Binding of thermostat %s not migrated to its UID, bind its sensor again", thermostatId);
      continue;
    }
    LOG_INFOF("\"%s\": binding migrated from id %s to UID %s",
              thermostat->name.c_str(),
              thermostatId,
              thermostat->uid.c_str());
    copyField(config.sensors_bindings[i][0], thermostat->uid.c_str());
    changed = true;
  }
  if (changed)
    saveConfig();
}

/**
 * Read the thermostats with the API chosen after the login. True if the FRITZ!Box answered the list of thermostats,
 * also an empty one.
 */
bool queryThermostats()
{
  ThermostatApi *api = fritzSession.api();
  return api != NULL && api->queryThermostats(thermostats);
}

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

    // the thermostat id is the unit UID with the REST API, the numeric device id with sh_control: when the API changes
    // (a FRITZ!OS update), forget the thermostats read with the other one
    static bool uidIds = false;
    ThermostatApi *api = fritzSession.api();
    if (api != NULL && api->usesUids() != uidIds)
    {
      thermostats.clear();
      uidIds = api->usesUids();
    }
    if (uidIds)
      migrateThermostatBindings();

    // Once per sync, when the FRITZ!Box answered the list of thermostats: also an empty one, and also when the offsets
    // are skipped, which can hold for all thermostats
    if (queryThermostats())
      feedWatchdog();
    auto timestamp = ntpClient.getEpochTime();
    for (auto iter = thermostats.begin(); iter != thermostats.end(); iter++)
    {
      iter->timestamp = timestamp;
      iter->referenceSensor = getThermostatBindingSensorId(iter->id.c_str());
      int binding = findThermostatBinding(iter->id.c_str());
      if (binding >= 0 && correctThermostat(*iter, binding, timestamp))
        updates = true;
    }

    if (updates)
    {
      queryThermostats();
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

void feedWatchdog()
{
  LOG_INFO("Watchdog reset");
  esp_task_wdt_reset();
}

/**
 * Whether FRITZ!Box host, user and password are set.
 */
bool fritzCredentialsConfigured()
{
  return config.fritz_host[0] != 0 && config.fritz_user[0] != 0 && config.fritz_pass[0] != 0;
}

void setupWatchdog()
{
  LOG_INFO("Watchdog setup");
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                    // add current thread to WDT watch
}

/**
 * Setup WiFi connection.
 */
void setupWIFI()
{
  LOG_INFO("WiFi setup");
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(config.wifi_ssid, config.wifi_pass);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  LOG_INFOF("WiFi connecting: %s", config.wifi_ssid);
  // a dot every half second, on the serial port only
  LOG_BEGIN(LOG_COLOR_INFO);
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++)
  {
    enableLed(i % 2);
    delay(500);
    Serial.print(".");
  }
  LOG_ENDLN();
  enableLed(false);
  LOG_SUCCESSF("WiFi ready: http://%s", WiFi.localIP().toString().c_str());
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
  ntpLastSyncMillis = millis();
}

/**
 * Sync the clock with the NTP server every NTP_SYNC_INTERVAL, or every NTP_RETRY_INTERVAL until a sync worked. Sensor
 * timestamps move with the clock, so that a step, such as the first sync after a failed one at boot, doesn't make
 * their readings look older or newer than they are.
 */
void syncClock()
{
  unsigned long interval = ntpClient.isTimeSet() ? NTP_SYNC_INTERVAL : NTP_RETRY_INTERVAL;
  if (millis() - ntpLastSyncMillis < interval * 1000UL)
    return;
  ntpLastSyncMillis = millis();

  unsigned long before = ntpClient.getEpochTime();
  if (!ntpClient.forceUpdate())
  {
    LOG_WARNING("NTP sync failed");
    return;
  }
  long step = (long)(ntpClient.getEpochTime() - before);
  if (step != 0)
  {
    LOG_INFOF("Clock moved by %ld s", step);
    for (int i = 0; i < sensorsCount; i++)
      sensors[i]->timestamp += step;
  }
}

/**
 * Whether a reading of a time on the clock is at most Sensor::OFFLINE_TIMEOUT old. A sensor can report while the caller
 * runs, so a reading a bit newer than now counts too.
 */
bool isRecentReading(unsigned long now, unsigned long timestamp)
{
  long age = (long)(now - timestamp);
  return age > -Sensor::OFFLINE_TIMEOUT && age <= Sensor::OFFLINE_TIMEOUT;
}

/**
 * Add a sample of each sensor and thermostat with a recent reading to the history. Waits for the first NTP sync, as the
 * history slots go by the clock.
 */
void recordHistory()
{
  if (!ntpClient.isTimeSet())
    return;
  unsigned long now = ntpClient.getEpochTime();
  unsigned long utc = now - NTP_TIME_OFFSET;

  for (int i = 0; i < sensorsCount; i++)
  {
    Sensor *sensor = sensors[i];
    if (isRecentReading(now, sensor->timestamp))
      history.addSensor(sensor->id, utc, (int32_t)sensor->temperature, sensor->humidity);
  }
  for (auto &thermostat : thermostats)
  {
    if (thermostat.temperatureValid && isRecentReading(now, thermostat.timestamp))
      history.addThermostat(
          thermostat.id, utc, (int32_t)thermostat.temperature, thermostat.targetTemperature, thermostat.offset);
  }
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
    LOG_ERROR("EEPROM init failed");
  }

  storage.get(0, config);

  // older versions could save a value that fills its field, without the terminating zero
  terminateField(config.wifi_ssid);
  terminateField(config.wifi_pass);
  terminateField(config.fritz_host);
  terminateField(config.fritz_user);
  terminateField(config.fritz_pass);
  for (int i = 0; i < MAX_SENSORS_COUNT; i++)
  {
    terminateField(config.sensors_bindings[i][0]);
    terminateField(config.sensors_bindings[i][1]);
  }

  bool signatureMismatch = config.signature != defaultConfig.signature;
  if (signatureMismatch)
  {
    LOG_WARNINGF("Config signature mismatch: %x != %x", config.signature, defaultConfig.signature);
  }

  config.signature = defaultConfig.signature;

  // use default value for wifi settings if it defined in code as user cant change it
  useDefaultIfNotDefined(config.wifi_ssid, defaultConfig.wifi_ssid, signatureMismatch, true);
  useDefaultIfNotDefined(config.wifi_pass, defaultConfig.wifi_pass, signatureMismatch, true);
  // for the rest params use default value only if it is changed by user
  useDefaultIfNotDefined(config.fritz_user, defaultConfig.fritz_user, signatureMismatch, false);
  useDefaultIfNotDefined(config.fritz_pass, defaultConfig.fritz_pass, signatureMismatch, false);
  useDefaultIfNotDefined(config.fritz_host, defaultConfig.fritz_host, signatureMismatch, false);

  bool bindingsReset = signatureMismatch || (defaultConfig.sensors_bindings[0][0][0] != 0);
  if (bindingsReset)
  {
    LOG_WARNING("Sensor bindings reset");
    memcpy(config.sensors_bindings, defaultConfig.sensors_bindings, sizeof(defaultConfig.sensors_bindings));
  }
  if (bindingsReset || config.offsets_signature != OFFSETS_SIGNATURE)
  {
    // saved by a version that didn't store the offsets the thermostats had before they were bound: assume 0 °C
    memcpy(config.sensors_bindings_offsets,
           defaultConfig.sensors_bindings_offsets,
           sizeof(defaultConfig.sensors_bindings_offsets));
    config.offsets_signature = OFFSETS_SIGNATURE;
  }

  saveConfig();
}

/**
 * Setup Arduino application.
 */
void setup()
{
  Serial.begin(115200);
  LOG_SUCCESSF("FritzGate %s starting", FRITZGATE_VERSION);

  setupWatchdog();
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

  if (fritzCredentialsConfigured())
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
 * Read an unsigned integer query parameter of an HTTP request line, such as hours in "GET /api/history?hours=24 HTTP/1.1".
 * Returns defaultValue if the parameter is missing, and 0 if its value doesn't start with a digit.
 */
unsigned long queryParameter(const char *requestLine, const char *name, unsigned long defaultValue)
{
  const char *end = strchr(requestLine, ' ');
  end = end ? strchr(end + 1, ' ') : NULL;
  const char *parameter = strchr(requestLine, '?');
  size_t nameLength = strlen(name);
  while (parameter != NULL && (end == NULL || parameter < end))
  {
    parameter++;
    if (strncmp(parameter, name, nameLength) == 0 && parameter[nameLength] == '=')
      return isdigit((unsigned char)parameter[nameLength + 1]) ? strtoul(parameter + nameLength + 1, NULL, 10) : 0;
    parameter = strchr(parameter, '&');
  }
  return defaultValue;
}

/**
 * Answer an API request with 400 Bad Request and an error message.
 */
void writeJsonError(Stream &client, const char *error)
{
  writeJsonHeaders(client, 400, "Bad Request");
  client.print("{\"error\":");
  printJsonString(client, error);
  client.print("}");
}

/**
 * Why the board started last, as a key that the web UI explains. The ESP32 keeps the cause of a reset through it.
 * The reset button and flashing count as power on; ESP_RST_TASK_WDT is the watchdog of setupWatchdog().
 */
const char *resetReason()
{
  switch (esp_reset_reason())
  {
  case ESP_RST_POWERON:
    return "power_on";
  case ESP_RST_EXT:
    return "external";
  case ESP_RST_SW:
    return "software";
  case ESP_RST_PANIC:
    return "crash";
  case ESP_RST_INT_WDT:
    return "interrupt_watchdog";
  case ESP_RST_TASK_WDT:
    return "task_watchdog";
  case ESP_RST_WDT:
    return "watchdog";
  case ESP_RST_DEEPSLEEP:
    return "deep_sleep";
  case ESP_RST_BROWNOUT:
    return "brownout";
  case ESP_RST_SDIO:
    return "sdio";
  default:
    return "unknown";
  }
}

/**
 * Write the answer of GET /api/diagnostics: the state of the board, and the kept log lines numbered after or later.
 */
void printDiagnostics(Print &out, uint32_t after)
{
  out.print("{");
  printJsonMember(out, "version", FRITZGATE_VERSION);
  // esp_timer counts microseconds in 64 bits, so unlike millis() it doesn't wrap after 49.7 days
  out.printf(",\"uptime\":%lu,", (unsigned long)(esp_timer_get_time() / 1000000));
  printJsonMember(out, "resetReason", resetReason());
  out.printf(",\"freeHeap\":%lu,\"minFreeHeap\":%lu,\"freePsram\":%lu,\"rssi\":%d,",
             (unsigned long)ESP.getFreeHeap(),
             (unsigned long)ESP.getMinFreeHeap(),
             (unsigned long)ESP.getFreePsram(),
             (int)WiFi.RSSI());
  printJsonMember(out, "fritzOsVersion", fritzSession.version());
  out.print(",\"fritzApi\":");
  ThermostatApi *api = fritzSession.api();
  if (api)
    printJsonString(out, api->name());
  else
    out.print("null");
  out.print(",\"log\":");
  printLog(out, after);
  out.print("}");
}

/**
 * This is the Arduino main loop function.
 */
void loop()
{
  bool fritzConfigured = fritzCredentialsConfigured();

  syncClock();

  // Fetch data from FritzBox every 30 seconds
  if (fritzConfigured && millis() - fritzSmartHomeControlLastRefreshMillis >= (FRITZGATE_REFRESH_TIMEOUT) * 1000UL)
  {
    fritzSmartHomeControlLastRefreshMillis = millis();
    syncFritzBox();
  }

  if (millis() - historyLastSampleMillis >= HISTORY_SAMPLE_INTERVAL * 1000UL)
  {
    historyLastSampleMillis = millis();
    recordHistory();
  }

  // Without FRITZ!Box credentials there is no sync to feed the watchdog, and the board only shows the sensors. A timer
  // of its own, so that the first sync still starts right after the credentials are set.
  if (!fritzConfigured && millis() - watchdogLastFeedMillis >= (FRITZGATE_REFRESH_TIMEOUT) * 1000UL)
  {
    watchdogLastFeedMillis = millis();
    feedWatchdog();
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
          // the diagnostics dialog asks every few seconds, its requests would fill the log it shows
          if (!startsWith(linebuf, "GET /api/diagnostics"))
            LOG_HTTPF("HTTP request: %s", linebuf);

          if (startsWith(linebuf, "GET /api/status"))
          {
            long timestamp = ntpClient.getEpochTime();
            writeJsonHeaders(client);
            client.print("{");
            printJsonMember(client, "version", FRITZGATE_VERSION);
            client.printf(",\"fritz\":{\"status\":\"%s\",", fritzStatus);
            printJsonMember(client, "host", config.fritz_host);
            client.print(",");
            printJsonMember(client, "user", config.fritz_user);
            client.print("},\"sensors\":[");
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
          else if (startsWith(linebuf, "GET /api/history"))
          {
            unsigned long hours = queryParameter(linebuf, "hours", 24);
            unsigned long step = queryParameter(linebuf, "step", HISTORY_INTERVAL);
            if (hours < 1 || hours > HISTORY_HOURS || step < HISTORY_INTERVAL || step % HISTORY_INTERVAL != 0 ||
                (hours * 60 * 60 + step - 1) / step > HISTORY_MAX_POINTS)
            {
              writeJsonError(client, "hours must be 1 to 720, step a multiple of 300 with at most 1000 points");
            }
            else
            {
              writeJsonHeaders(client);
              BufferedPrint<1024> out(client);
              history.print(out, ntpClient.getEpochTime() - NTP_TIME_OFFSET, hours, step);
              out.flush();
            }
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "GET /api/diagnostics"))
          {
            writeJsonHeaders(client);
            BufferedPrint<1024> out(client);
            printDiagnostics(out, (uint32_t)queryParameter(linebuf, "after", 0));
            out.flush();
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
            DynamicJsonDocument fritzauth(512);
            DeserializationError error = deserializeJson(fritzauth, client, DeserializationOption::NestingLimit(2));

            const char *host = fritzauth["host"] | "";
            const char *user = fritzauth["user"] | "";
            const char *pass = fritzauth["pass"] | "";

            if (error || host[0] == 0 || user[0] == 0 || pass[0] == 0)
            {
              writeJsonError(client, "host, user and pass are required");
            }
            else if (!fitsField(config.fritz_host, host) ||
                     !fitsField(config.fritz_user, user) ||
                     !fitsField(config.fritz_pass, pass))
            {
              writeJsonError(client, "host, user and pass can have at most 31 bytes each");
            }
            else
            {
              copyField(config.fritz_host, host);
              copyField(config.fritz_user, user);
              copyField(config.fritz_pass, pass);
              fritzStatus = FRITZ_STATUS_CONFIGURED;
              saveConfig();
              fritzSession.configure(config.fritz_host, 80, config.fritz_user, config.fritz_pass);

              writeJsonHeaders(client);
              client.print("{}");
            }
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "POST /api/thermostat"))
          {
            DynamicJsonDocument thermostat(200);
            DeserializationError error = deserializeJson(thermostat, client, DeserializationOption::NestingLimit(2));

            const char *thermostatId = thermostat["id"] | "";
            const char *referenceSensor = thermostat["referenceSensor"] | "";
            if (error || !updateThermostatBinding(thermostatId, referenceSensor))
            {
              writeJsonError(client, "unknown thermostat, id too long or no free binding");
              client.flush();
              break;
            }
            saveConfig();

            writeJsonHeaders(client);

            client.printf("{\"sensors_bindings\":[");
            int count = countThermostatBindings();
            for (int i = 0; i < count; i++)
            {
              client.print(i == 0 ? "[" : ",[");
              printJsonString(client, config.sensors_bindings[i][0]);
              client.print(",");
              printJsonString(client, config.sensors_bindings[i][1]);
              client.print("]");
            }
            client.print("]}");
            client.flush();
            break;
          }
          else if (startsWith(linebuf, "GET /api/") || startsWith(linebuf, "POST /api/") || startsWith(linebuf, "GET /favicon.ico"))
          {
            writeHttpStatus(client, 404, "Not Found");
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
