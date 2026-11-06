#include "history.h"
#include <esp_heap_caps.h>
#include "../json.h"
#include "../logging.h"

namespace
{
  int32_t clamp(int32_t value, int32_t low, int32_t high)
  {
    return value < low ? low : (value > high ? high : value);
  }

  // Mean of count values, rounded half away from zero
  int32_t mean(int32_t sum, int32_t count)
  {
    return (sum >= 0 ? sum + count / 2 : sum - count / 2) / count;
  }

  // Print a value in 0.1 units as a JSON number: 215 as 21.5, -5 as -0.5, 220 as 22
  void printTenths(Print &out, int32_t value)
  {
    if (value < 0)
    {
      out.print('-');
      value = -value;
    }
    out.print((long)(value / 10));
    if (value % 10 != 0)
    {
      out.print('.');
      out.print((long)(value % 10));
    }
  }

  // Samples of the slots of one step
  struct StepSummary
  {
    int32_t temperatureSum = 0;
    int32_t temperatureCount = 0;
    int32_t humiditySum = 0;
    int32_t humidityCount = 0;
    // The newest slot with samples, for the thermostat settings
    const HistorySample *last = NULL;
  };

  // The values one series prints
  enum HistoryField
  {
    FIELD_TEMPERATURE,
    FIELD_HUMIDITY,
    FIELD_TARGET,
    FIELD_OFFSET,
  };

  const char *fieldName(HistoryField field)
  {
    switch (field)
    {
    case FIELD_TEMPERATURE:
      return "temperature";
    case FIELD_HUMIDITY:
      return "humidity";
    case FIELD_TARGET:
      return "targetTemperature";
    default:
      return "offset";
    }
  }

  // The samples of a series in the slots of one step that the ring still keeps
  StepSummary summarize(const HistorySeries &series, uint32_t newestSlot, uint32_t bucket, uint32_t slotsPerStep)
  {
    StepSummary step;
    if (newestSlot == 0)
      return step;
    uint32_t oldestSlot = newestSlot >= HISTORY_SLOTS ? newestSlot - HISTORY_SLOTS + 1 : 0;
    for (uint32_t slot = bucket * slotsPerStep; slot < (bucket + 1) * slotsPerStep; slot++)
    {
      if (slot < oldestSlot || slot > newestSlot)
        continue;
      const HistorySample &sample = series.samples[slot % HISTORY_SLOTS];
      if (sample.temperature == HISTORY_NO_VALUE)
        continue;
      step.temperatureSum += sample.temperature;
      step.temperatureCount++;
      if (series.kind == HISTORY_SENSOR && sample.humidity != HISTORY_NO_HUMIDITY)
      {
        step.humiditySum += sample.humidity;
        step.humidityCount++;
      }
      step.last = &sample;
    }
    return step;
  }

  bool hasValue(const StepSummary &step, HistoryField field)
  {
    switch (field)
    {
    case FIELD_TEMPERATURE:
      return step.temperatureCount > 0;
    case FIELD_HUMIDITY:
      return step.humidityCount > 0;
    default:
      return step.last != NULL;
    }
  }

  // Print a value of a step, or null if the step has none
  void printField(Print &out, const StepSummary &step, HistoryField field)
  {
    if (!hasValue(step, field))
    {
      out.print("null");
      return;
    }
    switch (field)
    {
    case FIELD_TEMPERATURE:
      return printTenths(out, mean(step.temperatureSum, step.temperatureCount));
    case FIELD_HUMIDITY:
      return printTenths(out, mean(step.humiditySum, step.humidityCount));
    case FIELD_TARGET:
      return printTenths(out, step.last->thermostat.target * 5);
    case FIELD_OFFSET:
      return printTenths(out, step.last->thermostat.offset);
    }
  }
}

void History::addSensor(const std::string &id, unsigned long utc, int32_t temperature, uint32_t humidity)
{
  HistorySeries *series = getSeries(id, HISTORY_SENSOR);
  if (series == NULL)
    return;
  HistorySample *sample = startSample(*series, utc);

  series->temperatureSum += temperature;
  series->temperatureCount++;
  sample->temperature = (int16_t)clamp(mean(series->temperatureSum, series->temperatureCount), -INT16_MAX, INT16_MAX);
  if (humidity > 0)
  {
    series->humiditySum += (int32_t)min(humidity, (uint32_t)1000);
    series->humidityCount++;
  }
  sample->humidity = series->humidityCount > 0
                         ? (uint16_t)mean(series->humiditySum, series->humidityCount)
                         : HISTORY_NO_HUMIDITY;
}

void History::addThermostat(const std::string &id, unsigned long utc, int32_t temperature, uint32_t target, int32_t offset)
{
  HistorySeries *series = getSeries(id, HISTORY_THERMOSTAT);
  if (series == NULL)
    return;
  HistorySample *sample = startSample(*series, utc);

  series->temperatureSum += temperature;
  series->temperatureCount++;
  sample->temperature = (int16_t)clamp(mean(series->temperatureSum, series->temperatureCount), -INT16_MAX, INT16_MAX);
  sample->thermostat.target = (uint8_t)min((target + 2) / 5, (uint32_t)UINT8_MAX);
  sample->thermostat.offset = (int8_t)clamp(offset, -INT8_MAX, INT8_MAX);
}

/**
 * The series of a sensor or thermostat. A new one gets its samples from PSRAM, all empty. Returns NULL if the series or
 * the PSRAM are used up.
 */
HistorySeries *History::getSeries(const std::string &id, HistoryKind kind)
{
  for (int i = 0; i < seriesCount; i++)
  {
    if (series[i].kind == kind && series[i].id == id)
      return &series[i];
  }

  HistorySample *samples = seriesCount < HISTORY_MAX_SERIES
                               ? (HistorySample *)heap_caps_malloc(HISTORY_SLOTS * sizeof(HistorySample), MALLOC_CAP_SPIRAM)
                               : NULL;
  if (samples == NULL)
  {
    if (!fullLogged)
      LOG_WARNINGF("History: no room for %s, %d series kept", id.c_str(), seriesCount);
    fullLogged = true;
    return NULL;
  }
  for (int i = 0; i < HISTORY_SLOTS; i++)
  {
    samples[i].temperature = HISTORY_NO_VALUE;
    samples[i].humidity = HISTORY_NO_HUMIDITY;
  }

  HistorySeries &added = series[seriesCount++];
  added.id = id;
  added.kind = kind;
  added.samples = samples;
  added.sumSlot = 0;
  LOG_INFOF("History: recording %s, %u bytes of PSRAM free", id.c_str(), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  return &added;
}

/**
 * The sample of the slot of a UTC time, to add a sample of the series to. Starts new sums for the mean when the slot
 * differs from the one the series added to last.
 */
HistorySample *History::startSample(HistorySeries &series, unsigned long utc)
{
  uint32_t slot = utc / HISTORY_INTERVAL;
  advance(slot);
  if (series.sumSlot != slot)
  {
    series.sumSlot = slot;
    series.temperatureSum = 0;
    series.temperatureCount = 0;
    series.humiditySum = 0;
    series.humidityCount = 0;
  }
  return &series.samples[slot % HISTORY_SLOTS];
}

/**
 * Make a slot the newest one, emptying the slots that the ring reuses for it. A clock that moved back by more than the
 * whole history makes the samples useless, so they are all emptied; a smaller move back keeps them.
 */
void History::advance(uint32_t slot)
{
  bool kept = newestSlot != 0 && slot + HISTORY_SLOTS > newestSlot;
  if (kept && slot <= newestSlot)
    return;

  uint32_t first = kept && slot - newestSlot < HISTORY_SLOTS ? newestSlot + 1 : slot - HISTORY_SLOTS + 1;
  for (int i = 0; i < seriesCount; i++)
  {
    for (uint32_t s = first; s <= slot; s++)
      series[i].samples[s % HISTORY_SLOTS].temperature = HISTORY_NO_VALUE;
  }
  newestSlot = slot;
}

void History::print(Print &out, unsigned long utc, unsigned long hours, unsigned long step)
{
  uint32_t lastBucket = utc / step;
  uint32_t count = min((hours * 60 * 60 + step - 1) / step, (unsigned long)lastBucket + 1);
  uint32_t firstBucket = lastBucket + 1 - count;
  uint32_t slotsPerStep = step / HISTORY_INTERVAL;

  out.printf("{\"last\":%lu,\"step\":%lu,", (unsigned long)lastBucket * step, step);
  for (HistoryKind kind : {HISTORY_SENSOR, HISTORY_THERMOSTAT})
  {
    out.print(kind == HISTORY_SENSOR ? "\"sensors\":[" : "],\"thermostats\":[");
    bool first = true;
    for (int i = 0; i < seriesCount; i++)
    {
      if (series[i].kind != kind)
        continue;
      if (!first)
        out.print(',');
      first = false;
      printSeries(out, series[i], firstBucket, count, slotsPerStep);
    }
  }
  out.print("]}");
}

void History::printSeries(Print &out, const HistorySeries &series, uint32_t firstBucket, uint32_t count, uint32_t slotsPerStep)
{
  out.print("{\"id\":");
  printJsonString(out, series.id.c_str());

  static const HistoryField sensorFields[] = {FIELD_TEMPERATURE, FIELD_HUMIDITY};
  static const HistoryField thermostatFields[] = {FIELD_TEMPERATURE, FIELD_TARGET, FIELD_OFFSET};
  bool sensor = series.kind == HISTORY_SENSOR;
  const HistoryField *fields = sensor ? sensorFields : thermostatFields;
  int fieldCount = sensor ? 2 : 3;

  uint32_t endBucket = firstBucket + count;
  for (int f = 0; f < fieldCount; f++)
  {
    // leave out the steps before the first one with a value
    uint32_t start = firstBucket;
    while (start < endBucket && !hasValue(summarize(series, newestSlot, start, slotsPerStep), fields[f]))
      start++;

    out.printf(",\"%s\":[", fieldName(fields[f]));
    for (uint32_t bucket = start; bucket < endBucket; bucket++)
    {
      if (bucket != start)
        out.print(',');
      printField(out, summarize(series, newestSlot, bucket, slotsPerStep), fields[f]);
    }
    out.print(']');
  }
  out.print('}');
}
