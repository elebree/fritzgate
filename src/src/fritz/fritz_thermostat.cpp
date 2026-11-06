#include "fritz_thermostat.h"
#include "../logging.h"
#include <ArduinoJson.h>
#include <map>
#include <StreamString.h>
#include <string>

// spaceAsPlus: form encoding, a space becomes "+"; in a URL path it must be "%20"
std::string FritzThermostat::urlEncode(const char *msg, bool spaceAsPlus)
{
    const char *hex = "0123456789ABCDEF";
    std::string encodedMsg = "";

    while (*msg != '\0')
    {
        if (*msg == ' ' && spaceAsPlus)
        {
            encodedMsg += '+';
        }
        else if (
            ('a' <= *msg && *msg <= 'z') || ('A' <= *msg && *msg <= 'Z') ||
            ('0' <= *msg && *msg <= '9') ||
            *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~')
        {
            encodedMsg += *msg;
        }
        else
        {
            encodedMsg += '%';
            encodedMsg += hex[(unsigned char)*msg >> 4];
            encodedMsg += hex[*msg & 0xf];
        }
        msg++;
    }
    return encodedMsg;
}

void FritzThermostat::printPostBody(Stream &stream, Thermostat &info, const char *sid)
{
    stream.printf("xhr=1");
    stream.printf("&sid=%s", sid);
    stream.printf("&device=%s", info.id.c_str());
    stream.printf("&view=");
    stream.printf("&back_to_page=%%2Fsmarthome%%2Fdevices.lua");
    stream.printf("&ule_device_name=%s", urlEncode(info.name.c_str()).c_str());

    // checkboxes: like a browser, post them only when checked, a missing field switches the option off
    if (info.buttonLocked)
        stream.printf("&locklocal=on");
    if (info.externalLocked)
        stream.printf("&lockuiapp=on");
    if (info.adaptiveHeating)
        stream.printf("&hkr_adaptheat=1");

    stream.printf("&Heiztemp=%g", info.targetTemperatureHigh / 10.0);
    stream.printf("&Absenktemp=%g", info.targetTemperatureLow / 10.0);
    stream.printf("&graphState=1"); // ??

    int i = 0;
    for (auto iter = info.weeklyTimetable.begin(); iter != info.weeklyTimetable.end(); iter++)
    {
        stream.printf("&timer_item_%d=%02d%02d%%3B%d%%3B%d",
                      i++,
                      iter->hours(),
                      iter->minutes(),
                      iter->enabled() ? 1 : 0, iter->daysOfWeekMask());
    }

    // FRITZ!Box disables a holiday posted without the holiday temperature. The browser posts it whenever a holiday
    // is enabled (the field is disabled while there is none), so do the same.
    const char *holiday = "&Holiday";
    int enabledHolidays = info.enabledHolidays();
    if (enabledHolidays > 0 && info.holidayTemperature > 0)
        stream.printf("&Holidaytemp=%g", info.holidayTemperature / 10.0);

    // the form takes the hours as the device JSON reports them ("10:00:00" is posted as 10); the JSON reports
    // "00:00:00" for every disabled holiday, whatever hour was posted
    for (int i = 1; i <= THERMOSTAT_HOLIDAYS; i++)
    {
        const YearlyPeriod &period = info.holidays[i - 1];
        stream.printf("%s%dStartDay=%d", holiday, i, period.startDay);
        stream.printf("%s%dStartMonth=%d", holiday, i, period.startMonth);
        stream.printf("%s%dStartHour=%d", holiday, i, period.startHour);
        stream.printf("%s%dEndDay=%d", holiday, i, period.endDay);
        stream.printf("%s%dEndMonth=%d", holiday, i, period.endMonth);
        stream.printf("%s%dEndHour=%d", holiday, i, period.endHour);
        stream.printf("%s%dEnabled=%d", holiday, i, period.enabled ? 1 : 0);
        stream.printf("%s%dID=%d", holiday, i, i);
    }
    stream.printf("%sEnabledCount=%d", holiday, enabledHolidays);

    const char *summer = "&Summer";
    stream.printf("%sStartDay=%d", summer, info.summer.startDay);
    stream.printf("%sStartMonth=%02d", summer, info.summer.startMonth);
    stream.printf("%sEndDay=%d", summer, info.summer.endDay);
    stream.printf("%sEndMonth=%02d", summer, info.summer.endMonth);
    stream.printf("%sEnabled=%d", summer, info.summer.enabled ? 1 : 0);

    const char *windowOpen = "&WindowOpen";
    stream.printf("%sTrigger=%d", windowOpen, info.windowOpenTrigger + 3);
    stream.printf("%sTimer=%d", windowOpen, info.windowOpenTimer);

    // FritzGate sets the offset of the thermostat's own sensor, so it switches from an external sensor set in
    // FRITZ!Box back to the own one: the reference sensor bound in FritzGate replaces it
    stream.printf("&tempsensor=own");
    stream.printf("&Roomtemp=%g", (info.temperature + info.offset) / 10.0);
    stream.printf("&ExtTempsensorID=tochoose");
    stream.printf("&Offset=%g", info.offset / 10.0);

    // the mail address field is disabled, so not posted, while the push service is off
    if (info.pushServiceEnabled)
    {
        stream.printf("&enabled=on");
        stream.printf("&mailto=%s", urlEncode(info.pushServiceMail.c_str()).c_str());
    }
    stream.printf("&apply=");
    stream.printf("&lang=en");
    stream.printf("&page=home_auto_hkr_edit");
}

// Read a yearly period from a REPETITIVE schedule action ("YYYY-MM-DD" dates, "HH:MM:SS" times)
static void parseYearlyPeriod(JsonObject schedule, JsonObject action, YearlyPeriod &period)
{
    JsonObject timeSetting = action["timeSetting"];
    int month, day;
    if (sscanf(timeSetting["startDate"] | "", "%*d-%d-%d", &month, &day) == 2)
    {
        period.startMonth = month;
        period.startDay = day;
    }
    if (sscanf(timeSetting["endDate"] | "", "%*d-%d-%d", &month, &day) == 2)
    {
        period.endMonth = month;
        period.endDay = day;
    }
    period.startHour = atoi(timeSetting["startTime"] | "0");
    period.endHour = atoi(timeSetting["endTime"] | "0");
    period.enabled = schedule["isEnabled"].as<bool>() && action["isEnabled"].as<bool>();
}

bool FritzThermostat::parseHolidayTemperature(Stream &stream, Thermostat &info)
{
    // the hidden input the edit form posts: <input type="hidden" name="Holidaytemp" value="16" ...>
    if (!stream.find("name=\"Holidaytemp\" value=\""))
    {
        LOG_ERROR("FRITZ holiday temperature missing");
        return false;
    }
    String value = stream.readStringUntil('"');
    value.replace(',', '.');
    double temperature = atof(value.c_str());
    // the form range, 7.5 is shown as "OFF" and 28.5 as "MAX"
    if (temperature < 7.5 || temperature > 28.5)
    {
        LOG_ERRORF("FRITZ holiday temperature invalid: %s", value.c_str());
        return false;
    }
    info.holidayTemperature = (uint32_t)(temperature * 10 + 0.5);
    return true;
}

String FritzThermostat::getPostBody(Thermostat &info, const char *sid)
{
    StreamString print;
    printPostBody(print, info, sid);
    return print;
}

std::string FritzThermostat::getUnitConfigurationPath(const Thermostat &info)
{
    // the unit UID is the AIN ("12345 0123456"), which sh_control reports as uid and actorIdentificationNumber
    const std::string &uid = info.uid.empty() ? info.serial : info.uid;
    return "/api/v0/smarthome/configuration/units/" + urlEncode(uid.c_str(), false);
}

String FritzThermostat::getOffsetBody(int32_t offset)
{
    // only the offset: the PUT is a partial update, so the schedule, holidays and other settings stay untouched
    StreamString print;
    print.printf("{\"interfaces\":{\"thermostatInterface\":{\"temperatureOffset\":"
                 "{\"sensorMode\":\"internal\",\"internalOffset\":%g}}}}",
                 offset / 10.0);
    return print;
}

// The thermostat with the given id, added if it is not known yet
static Thermostat &findOrAddThermostat(std::vector<Thermostat> &thermostats, const char *id)
{
    for (Thermostat &thermostat : thermostats)
    {
        if (thermostat.id == id)
            return thermostat;
    }
    thermostats.push_back(Thermostat());
    thermostats.back().id = id;
    return thermostats.back();
}

// Temperature in 0.1 °C of a REST API temperature {"mode": ..., "celsius": ...}. "off" is 0, which FritzGate takes as
// heating off; "on" (valve fully open) is 28.5 °C, which the edit form shows as "MAX"; "unknown" is 0 as well.
static uint32_t parseRestTemperature(JsonObject temperature)
{
    const char *mode = temperature["mode"] | "";
    if (strcmp(mode, "temperature") == 0)
        return (uint32_t)lround((temperature["celsius"] | 0.0) * 10);
    if (strcmp(mode, "on") == 0)
        return 285;
    return 0;
}

bool FritzThermostat::parseRestUnits(const String &json, std::vector<Thermostat> &thermostats, std::vector<std::string> &uids)
{
    StaticJsonDocument<256> filter;
    filter[0]["UID"] = true;
    filter[0]["isGroupUnit"] = true;
    // an object filter keeps thermostatInterface as an object, even an empty one, so thermostats can be told apart
    filter[0]["interfaces"]["thermostatInterface"]["state"] = true;

    DynamicJsonDocument units(json.length() / 2 + 1024);
    auto error = deserializeJson(units, json, DeserializationOption::Filter(filter));
    if (error)
    {
        LOG_ERRORF("FRITZ units JSON failed: %s", error.c_str());
        return false;
    }
    if (!units.is<JsonArray>())
    {
        LOG_ERROR("FRITZ units list missing");
        return false;
    }

    for (JsonObject unit : units.as<JsonArray>())
    {
        const char *uid = unit["UID"] | "";
        if (uid[0] == 0 || unit["isGroupUnit"].as<bool>() || unit["interfaces"]["thermostatInterface"].isNull())
            continue;
        Thermostat &info = findOrAddThermostat(thermostats, uid);
        info.uid = uid;
        uids.push_back(uid);
    }
    return true;
}

bool FritzThermostat::parseRestDevices(const String &json, std::vector<Thermostat> &thermostats)
{
    StaticJsonDocument<256> filter;
    filter[0]["unitUids"] = true;
    filter[0]["ain"] = true;
    filter[0]["productName"] = true;
    filter[0]["batteryValue"] = true;

    DynamicJsonDocument devices(json.length() / 2 + 1024);
    auto error = deserializeJson(devices, json, DeserializationOption::Filter(filter));
    if (error)
    {
        LOG_ERRORF("FRITZ devices JSON failed: %s", error.c_str());
        return false;
    }

    for (JsonObject device : devices.as<JsonArray>())
    {
        for (JsonVariant unitUid : device["unitUids"].as<JsonArray>())
        {
            const char *uid = unitUid | "";
            for (Thermostat &thermostat : thermostats)
            {
                if (thermostat.uid != uid)
                    continue;
                thermostat.model = device["productName"] | "";
                thermostat.serial = device["ain"] | "";
                thermostat.battery = device["batteryValue"] | 0;
            }
        }
    }
    return true;
}

bool FritzThermostat::parseRestUnitConfiguration(const String &json, Thermostat &info)
{
    StaticJsonDocument<512> filter;
    filter["name"] = true;
    JsonObject temperatureFilter = filter["interfaces"].createNestedObject("temperatureInterface");
    temperatureFilter["state"] = true;
    temperatureFilter["celsius"] = true;
    JsonObject thermostatFilter = filter["interfaces"].createNestedObject("thermostatInterface");
    thermostatFilter["setPointTemperature"] = true;
    thermostatFilter["comfortTemperature"] = true;
    thermostatFilter["reducedTemperature"] = true;
    thermostatFilter["temperatureOffset"]["sensorMode"] = true;
    thermostatFilter["temperatureOffset"]["internalOffset"] = true;
    thermostatFilter["temperatureOffset"]["externalSensorUid"] = true;

    // the filter drops the schedule and the temperature statistics, which make most of the 3 KB
    DynamicJsonDocument unit(2048);
    auto error = deserializeJson(unit, json, DeserializationOption::Filter(filter));
    if (error)
    {
        LOG_ERRORF("FRITZ unit JSON failed: %s", error.c_str());
        return false;
    }

    JsonObject thermostat = unit["interfaces"]["thermostatInterface"].as<JsonObject>();
    JsonObject offset = thermostat["temperatureOffset"].as<JsonObject>();
    if (offset.isNull())
    {
        LOG_ERRORF("FRITZ temperature offset missing: %s", info.id.c_str());
        return false;
    }

    const char *name = unit["name"];
    if (name)
        info.name = name;
    info.targetTemperature = parseRestTemperature(thermostat["setPointTemperature"].as<JsonObject>());
    info.targetTemperatureHigh = parseRestTemperature(thermostat["comfortTemperature"].as<JsonObject>());
    info.targetTemperatureLow = parseRestTemperature(thermostat["reducedTemperature"].as<JsonObject>());

    // FritzGate sets the offset of the thermostat's own sensor, so the PUT switches from an external sensor set in
    // FRITZ!Box back to the own one (sensorMode "internal")
    bool external = strcmp(offset["sensorMode"] | "", "external") == 0;
    info.temperatureSensor = external ? "external" : "own";
    info.externalTemperatureSensorId = external ? (offset["externalSensorUid"] | "") : "tochoose";
    info.offset = lround((offset["internalOffset"] | 0.0) * 10);

    // celsius is the temperature FRITZ!Box shows, with the offset, like currentInCelsius of sh_control. Read it with
    // the offset, so that both are from the same moment. While an external sensor is used, it may be the reading of
    // that sensor: then the first offset is off, and the next sync corrects it after the PUT switched to the own sensor.
    JsonObject temperature = unit["interfaces"]["temperatureInterface"].as<JsonObject>();
    JsonVariant celsius = temperature["celsius"];
    info.temperatureValid = strcmp(temperature["state"] | "", "valid") == 0 && !celsius.isNull();
    if (info.temperatureValid)
        info.temperature = lround(celsius.as<double>() * 10) - info.offset;
    return true;
}

bool FritzThermostat::parseDevices(HTTPClient &http, std::vector<Thermostat> &thermostats)
{
    const char *_LOWER_TEMPERATURE = "LOWER_TEMPERATURE";
    const char *_skills = "skills";
    const char *_temperature = "temperature";
    const char *_temperatureDropDetection = "temperatureDropDetection";
    const char *_UPPER_TEMPERATURE = "UPPER_TEMPERATURE";
    const char *_usedTempSensor = "usedTempSensor";

    auto stream = http.getStream();
    std::vector<Thermostat> infoList;
    // if (!find(stream, "\"devices\":")) {
    if (!stream.find("\"devices\":"))
    {
        LOG_ERROR("FRITZ devices block missing");
        return false;
    }

    DynamicJsonDocument device(15000);
    for (int i = 0; stream.find(i == 0 ? "[" : ",", 1); i++)
    {
        if (i == 0)
        {
            // FRITZ!Box without devices: "devices": []. peek() doesn't wait for the next character, so wait for it.
            for (unsigned long start = millis(); stream.available() == 0 && millis() - start < 1000;)
                delay(1);
            if (stream.peek() == ']')
                break;
        }

        // Thermostat info;
        std::map<long, int> weekly;
        // Deserialize the JSON document
        auto error = deserializeJson(device, stream, DeserializationOption::NestingLimit(14));

        // Test if parsing succeeds.
        if (error)
        {
            LOG_ERRORF("FRITZ devices JSON failed: %s", error.c_str());
            return false;
        }

        JsonArray units = device["units"];

        // prevents parsing, but cause error
        // [298153][E][WiFiClient.cpp:517] flush(): fail on fd 50, errno: 128, "Socket is not connected"
        if (units.size() == 0)
        {
            stream.stop();
            break;
        }

        Thermostat *info = NULL;
        auto id = std::to_string(device["id"].as<int>());

        // get thermostat info or create a new one
        for (auto iter = thermostats.begin(); iter != thermostats.end(); iter++)
        {
            if (iter->id == id)
            {
                info = &(*iter);
                break;
            }
        }

        if (info == NULL)
        {
            thermostats.push_back(Thermostat());
            info = &thermostats.back();
        }

        info->id = std::to_string(device["id"].as<int>());
        bool isThermostat = false;
        // the device's own temperature sensor unit, reported also while FRITZ!Box uses an external sensor
        JsonObject ownTemperature;
        for (JsonArray::iterator unit = units.begin(); unit != units.end(); unit += 1)
        {
            JsonArray skills = (*unit)[_skills];
            for (JsonArray::iterator skill = skills.begin(); skill != skills.end(); skill += 1)
            {
                if (strcmp((*skill)["type"], "SmartHomeBattery") == 0)
                {
                    info->battery = (*skill)["chargeLevelInPercent"].as<int>();
                }
                else if (strcmp((*skill)["type"], "SmartHomeTemperatureSensor") == 0)
                {
                    ownTemperature = skill->as<JsonObject>();
                }
                else if (strcmp((*skill)["type"], "SmartHomeThermostat") == 0)
                {
                    isThermostat = true;
                    info->model = device["model"].as<const char *>();
                    info->name = device["displayName"].as<const char *>();
                    const char *serial = device["actorIdentificationNumber"].as<const char *>();
                    info->serial = serial ? serial : "";
                    info->uid = device["uid"] | "";
                    info->windowOpenTimer = (*skill)[_temperatureDropDetection]["doNotHeatOffsetInMinutes"].as<int>();
                    info->windowOpenTrigger = (*skill)[_temperatureDropDetection]["sensitivity"].as<int>();

                    // printf("  targetTemp: %d\r\n", (*skill)["targetTemp"].as<int>());
                    info->temperatureSensor = (*skill)[_usedTempSensor]["id"].as<int>() == device["id"].as<int>() ? "own" : "unknown";
                    info->externalTemperatureSensorId = (*skill)[_usedTempSensor]["id"].as<int>() == device["id"].as<int>() ? "tochoose" : std::to_string(device["id"].as<int>()).c_str();
                    info->adaptiveHeating = (*skill)["adaptivHeating"]["isEnabled"].as<bool>();

                    info->buttonLocked = false;
                    info->externalLocked = false;
                    for (JsonObject control : (*unit)["interactionControls"].as<JsonArray>())
                    {
                        const char *controlName = control["devControlName"] | "";
                        if (strcmp(controlName, "BUTTON") == 0)
                            info->buttonLocked = control["isLocked"].as<bool>();
                        else if (strcmp(controlName, "EXTERNAL") == 0)
                            info->externalLocked = control["isLocked"].as<bool>();
                    }
                    info->pushServiceEnabled = device["pushService"]["isEnabled"].as<bool>();
                    info->pushServiceMail = device["pushService"]["mailAddress"] | "";

                    info->targetTemperature = (*skill)["targetTemp"].as<double>() * 10;
                    JsonArray presets = (*skill)["presets"];
                    for (JsonArray::iterator preset = presets.begin(); preset != presets.end(); preset += 1)
                    {
                        if (strcmp((*preset)["name"], _LOWER_TEMPERATURE) == 0)
                        {
                            info->targetTemperatureLow = (*preset)[_temperature].as<double>() * 10;
                        }
                        else if (strcmp((*preset)["name"], _UPPER_TEMPERATURE) == 0)
                        {
                            info->targetTemperatureHigh = (*preset)[_temperature].as<double>() * 10;
                        }
                    }

                    // rebuild the timetable from scratch, otherwise switch points removed in FRITZ!Box survive in the
                    // cached thermostat and are posted back with the next offset update
                    info->weeklyTimetable.clear();
                    JsonArray timeSchedules = (*skill)["timeControl"]["timeSchedules"];
                    for (JsonArray::iterator timeSchedule = timeSchedules.begin(); timeSchedule != timeSchedules.end(); timeSchedule += 1)
                    {
                        if (strcmp((*timeSchedule)["kind"], "WEEKLY_TIMETABLE") == 0)
                        {

                            JsonArray actions = (*timeSchedule)["actions"];
                            for (JsonArray::iterator action = actions.begin(); action != actions.end(); action += 1)
                            {
                                const char *daysOfWeek = "MONTUEWEDTHUFRISATSUN";

                                int dayOfWeek = (strstr(daysOfWeek, (*action)["timeSetting"]["dayOfWeek"]) - daysOfWeek) / 3;

                                auto startTime = (*action)["timeSetting"]["startTime"].as<const char *>();
                                auto presetName = (*action)["description"]["presetTemperature"]["name"].as<const char *>();
                                auto hours = atoi(startTime);
                                auto minutes = atoi(startTime + 3);
                                bool enabled = (strcmp(presetName, _UPPER_TEMPERATURE) == 0 ? 0x10000 : 0);
                                int x = 10;
                                auto action1 = std::find_if(info->weeklyTimetable.begin(), info->weeklyTimetable.end(), [hours, minutes, enabled](const PresetAction element)
                                                            { return (element.hours() == hours && element.minutes() == minutes && element.enabled() == enabled); });
                                if (action1 == info->weeklyTimetable.end())
                                {
                                    info->weeklyTimetable.push_back(PresetAction(hours, minutes, dayOfWeek, enabled));
                                }
                                else
                                {
                                    action1->addDayOfWeek(dayOfWeek);
                                }
                            }
                        }
                        else if ((*timeSchedule)["name"] == "HOLIDAYS")
                        {
                            JsonArray actions = (*timeSchedule)["actions"];
                            for (int h = 0; h < THERMOSTAT_HOLIDAYS; h++)
                            {
                                if (h < actions.size())
                                    parseYearlyPeriod(*timeSchedule, actions[h], info->holidays[h]);
                                else
                                    info->holidays[h].enabled = false;
                            }
                        }
                        else if ((*timeSchedule)["name"] == "SUMMER_TIME")
                        {
                            parseYearlyPeriod(*timeSchedule, (*timeSchedule)["actions"][0], info->summer);
                        }
                    }
                }
            }
        }

        if (isThermostat)
        {
            // Temperature and offset of the thermostat's own sensor, not of usedTempSensor (they are the same while
            // the thermostat uses its own sensor): FritzGate sets the offset of the own sensor and switches back to
            // it from an external sensor set in FRITZ!Box.
            // FRITZ!OS 8.26 can leave out currentInCelsius (seen for one thermostat in a capture); the offset can't
            // be computed then, so the temperature is marked invalid.
            info->temperatureValid = false;
            if (!ownTemperature.isNull())
            {
                info->offset = ownTemperature["offset"].as<double>() * 10;
                JsonVariant current = ownTemperature["currentInCelsius"];
                if (!current.isNull())
                {
                    info->temperature = current.as<double>() * 10 - info->offset;
                    info->temperatureValid = true;
                }
            }
        }

        std::sort(info->weeklyTimetable.begin(), info->weeklyTimetable.end(), [](PresetAction &a, PresetAction &b)
                  { return a.order() < b.order(); });
    }
    return true;
}
