#pragma once
#include <Stream.h>
#include <string>
#include <vector>
#include "sensor.h"

class PresetAction
{
private:
    uint16_t time;
    uint8_t state;

public:
    PresetAction(int hours, int minutes, int8_t dayOfWeek, bool enabled)
        : time(((int16_t)hours) << 8 | minutes),
          state(((1 << dayOfWeek) & 0x7F) | (enabled ? 0x80 : 0x00))
    {
    }

    uint8_t hours() const;
    uint8_t minutes() const;
    bool enabled() const;
    int order() const;
    uint8_t daysOfWeekMask() const;
    void addDayOfWeek(int8_t dayOfWeek);
    uint8_t firstDay() const;
};

// number of holiday periods a FRITZ!DECT thermostat supports
#define THERMOSTAT_HOLIDAYS 4

// yearly recurring period (holiday or summer) as reported by FRITZ!Box
struct YearlyPeriod
{
    uint8_t startDay;
    uint8_t startMonth;
    uint8_t startHour;
    uint8_t endDay;
    uint8_t endMonth;
    uint8_t endHour;
    bool enabled;
};

class Thermostat : public Sensor
{
public:
    int windowOpenTimer;
    int windowOpenTrigger;
    std::string temperatureSensor;
    std::string externalTemperatureSensorId;
    bool adaptiveHeating = false;
    uint32_t targetTemperature = 0;
    uint32_t targetTemperatureLow = 0;
    uint32_t targetTemperatureHigh = 0;
    int32_t offset = 0;
    unsigned long offsetTimestamp;
    std::vector<PresetAction> weeklyTimetable;
    // FRITZ!Box defaults, posted only if the device JSON has no holiday / summer schedule
    YearlyPeriod holidays[THERMOSTAT_HOLIDAYS] = {
        {11, 12, 0, 25, 12, 0, false},
        {11, 12, 0, 25, 12, 0, false},
        {11, 12, 0, 25, 12, 0, false},
        {11, 12, 0, 25, 12, 0, false}};
    YearlyPeriod summer = {1, 6, 0, 31, 8, 0, true};
    std::string referenceSensor;

protected:
    virtual void SerializeBody(Stream &client, long timestamp, int indentLevel);
};