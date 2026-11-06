#pragma once
#include <Print.h>
#include <stdint.h>
#include <string>

// Seconds per history slot: 5 minutes. Each slot holds the mean of the samples taken in it.
#define HISTORY_INTERVAL (5 * 60)

// Hours of history kept: 30 days
#define HISTORY_HOURS (30 * 24)

// Slots per series: 8640
#define HISTORY_SLOTS (HISTORY_HOURS * 60 * 60 / HISTORY_INTERVAL)

// Series kept at most, sensors and thermostats together: twice as many as FritzGate tracks sensors (MAX_SENSORS_COUNT).
// Each takes HISTORY_SLOTS * 4 bytes (34 KB) of PSRAM, allocated when it gets its first sample.
#define HISTORY_MAX_SERIES 40

// Points that one /api/history answer can have per value
#define HISTORY_MAX_POINTS 1000

// Temperature of a slot without samples
#define HISTORY_NO_VALUE INT16_MIN

// Humidity of a sensor that reports none
#define HISTORY_NO_HUMIDITY UINT16_MAX

// Settings of a thermostat in a slot: the last ones sampled
struct ThermostatSettings
{
  // Target temperature in 0.5 °C, 0 when heating is off
  uint8_t target;
  // Offset in 0.1 °C
  int8_t offset;
};

// One slot of a series, 4 bytes
struct HistorySample
{
  // Temperature in 0.1 °C, HISTORY_NO_VALUE for a slot without samples
  int16_t temperature;
  union
  {
    // Sensors: humidity in 0.1 %, HISTORY_NO_HUMIDITY for a sensor that reports none
    uint16_t humidity;
    // Thermostats
    ThermostatSettings thermostat;
  };
};

enum HistoryKind : uint8_t
{
  HISTORY_SENSOR,
  HISTORY_THERMOSTAT,
};

// Readings of one sensor or thermostat
struct HistorySeries
{
  std::string id;
  HistoryKind kind;
  // HISTORY_SLOTS samples in PSRAM, a ring indexed by the slot number (UTC seconds / HISTORY_INTERVAL)
  HistorySample *samples;

  // Sums of the samples taken in the newest slot written, for its mean
  uint32_t sumSlot;
  int32_t temperatureSum;
  uint16_t temperatureCount;
  int32_t humiditySum;
  uint16_t humidityCount;
};

/**
 * Readings of the sensors and thermostats of the last HISTORY_HOURS, in 5-minute slots. The samples live in PSRAM, which
 * is free while FritzGate runs (the camera only uses it to read the WiFi QR code before it restarts), and are lost on
 * restart. Not thread-safe: add samples and print from the same task.
 */
class History
{
public:
  // Add a sample of a sensor at a UTC time: temperature in 0.1 °C, humidity in 0.1 % (0 if the sensor has none)
  void addSensor(const std::string &id, unsigned long utc, int32_t temperature, uint32_t humidity);

  // Add a sample of a thermostat at a UTC time: temperature and offset in 0.1 °C, target temperature in 0.1 °C or 0
  void addThermostat(const std::string &id, unsigned long utc, int32_t temperature, uint32_t target, int32_t offset);

  /**
   * Print the history of the last hours as JSON, one point per step seconds (a multiple of HISTORY_INTERVAL): the mean
   * temperature and humidity of the slots in a step, and the last target temperature and offset. Steps start at
   * multiples of step seconds in UTC. Each array ends with the current step, which starts at "last", and starts with its
   * first step that has a value, so arrays can differ in length; later steps without samples are null.
   */
  void print(Print &out, unsigned long utc, unsigned long hours, unsigned long step);

private:
  HistorySeries series[HISTORY_MAX_SERIES] = {};
  int seriesCount = 0;
  // Number of the newest slot, 0 before the first sample
  uint32_t newestSlot = 0;
  // Logged once when the series or PSRAM run out
  bool fullLogged = false;

  HistorySeries *getSeries(const std::string &id, HistoryKind kind);
  HistorySample *startSample(HistorySeries &series, unsigned long utc);
  void advance(uint32_t slot);
  void printSeries(Print &out, const HistorySeries &series, uint32_t firstBucket, uint32_t count, uint32_t slotsPerStep);
};
