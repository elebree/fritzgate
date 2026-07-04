#include "fritz_thermostat.h"
#include "../logging.h"
#include <ArduinoJson.h>
#include <map>
#include <StreamString.h>
#include <string>

std::string FritzThermostat::urlEncode(const char *msg)
{
    const char *hex = "0123456789ABCDEF";
    std::string encodedMsg = "";

    while (*msg != '\0')
    {
        if (*msg == ' ')
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
    stream.printf("&hkr_adaptheat=%d", info.adaptiveHeating ? 1 : 0);
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

    const char *holiday = "&Holiday";
    for (int i = 1; i <= 4; i++)
    {
        stream.printf("%s%dStartDay=11", holiday, i);
        stream.printf("%s%dStartMonth=12", holiday, i);
        stream.printf("%s%dStartHour=1", holiday, i);
        stream.printf("%s%dEndDay=25", holiday, i);
        stream.printf("%s%dEndMonth=12", holiday, i);
        stream.printf("%s%dEndHour=1", holiday, i);
        stream.printf("%s%dEnabled=0", holiday, i);
        stream.printf("%s%dID=%d", holiday, i, i);
    }
    stream.printf("%sEnabledCount=0", holiday);

    const char *summer = "&Summer";
    stream.printf("%sStartDay=1", summer);
    stream.printf("%sStartMonth=06", summer);
    stream.printf("%sEndDay=31", summer);
    stream.printf("%sEndMonth=08", summer);
    stream.printf("%sEnabled=1", summer);

    const char *windowOpen = "&WindowOpen";
    stream.printf("%sTrigger=%d", windowOpen, info.windowOpenTrigger + 3);
    stream.printf("%sTimer=%d", windowOpen, info.windowOpenTimer);

    stream.printf("&tempsensor=%s", info.temperatureSensor.c_str());
    stream.printf("&Roomtemp=%g", (info.temperature + info.offset) / 10.0);
    stream.printf("&ExtTempsensorID=%s", info.externalTemperatureSensorId.c_str());
    stream.printf("&Offset=%g", info.offset / 10.0);
    stream.printf("&apply=");
    stream.printf("&lang=en");
    stream.printf("&page=home_auto_hkr_edit");
}

String FritzThermostat::getPostBody(Thermostat &info, const char *sid)
{
    StreamString print;
    printPostBody(print, info, sid);
    return print;
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
        for (JsonArray::iterator unit = units.begin(); unit != units.end(); unit += 1)
        {
            JsonArray skills = (*unit)[_skills];
            for (JsonArray::iterator skill = skills.begin(); skill != skills.end(); skill += 1)
            {
                if (strcmp((*skill)["type"], "SmartHomeBattery") == 0)
                {
                    info->battery = (*skill)["chargeLevelInPercent"].as<int>();
                }
                else if (strcmp((*skill)["type"], "SmartHomeThermostat") == 0)
                {
                    info->model = device["model"].as<const char *>();
                    info->name = device["displayName"].as<const char *>();
                    const char *serial = device["actorIdentificationNumber"].as<const char *>();
                    info->serial = serial ? serial : "";
                    info->windowOpenTimer = (*skill)[_temperatureDropDetection]["doNotHeatOffsetInMinutes"].as<int>();
                    info->windowOpenTrigger = (*skill)[_temperatureDropDetection]["sensitivity"].as<int>();

                    // printf("  targetTemp: %d\r\n", (*skill)["targetTemp"].as<int>());
                    info->temperatureSensor = (*skill)[_usedTempSensor]["id"].as<int>() == device["id"].as<int>() ? "own" : "unknown";
                    info->externalTemperatureSensorId = (*skill)[_usedTempSensor]["id"].as<int>() == device["id"].as<int>() ? "tochoose" : std::to_string(device["id"].as<int>()).c_str();
                    info->adaptiveHeating = (*skill)["adaptivHeating"]["isEnabled"].as<bool>();
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

                    JsonArray sensorSkills = (*skill)[_usedTempSensor][_skills];
                    for (JsonArray::iterator sensorSkill = sensorSkills.begin(); sensorSkill != sensorSkills.end(); sensorSkill += 1)
                    {
                        info->offset = (*sensorSkill)["offset"].as<double>() * 10;
                        info->temperature = (*sensorSkill)["currentInCelsius"].as<double>() * 10 - info->offset;
                    }

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
                    }
                }
            }
        }

        std::sort(info->weeklyTimetable.begin(), info->weeklyTimetable.end(), [](PresetAction &a, PresetAction &b)
                  { return a.order() < b.order(); });
    }
    return true;
}
