#pragma once
#include <HTTPClient.h>
#include <Print.h>
#include "../models/thermostat.h"

class FritzThermostat
{
private:
  std::string static urlEncode(const char *msg, bool spaceAsPlus = true);

public:
  void static printPostBody(Stream &stream, Thermostat &info, const char *sid);
  String static getPostBody(Thermostat &info, const char *sid);
  // Smart Home REST API (FRITZ!OS 8.20+): configuration path of the unit, read with GET and changed with PUT
  std::string static getUnitConfigurationPath(const Thermostat &info);
  // Smart Home REST API: body of the PUT that sets only the offset
  String static getOffsetBody(int32_t offset);
  bool static parseDevices(HTTPClient &http, std::vector<Thermostat> &thermostats);
  bool static parseHolidayTemperature(Stream &stream, Thermostat &info);
  // Smart Home REST API: add the thermostat units of GET overview/units to thermostats, and their UIDs to uids
  bool static parseRestUnits(const String &json, std::vector<Thermostat> &thermostats, std::vector<std::string> &uids);
  // Smart Home REST API: model, AIN and battery of the thermostats from GET overview/devices
  bool static parseRestDevices(const String &json, std::vector<Thermostat> &thermostats);
  // Smart Home REST API: state of a thermostat from GET configuration/units/{UID}
  bool static parseRestUnitConfiguration(const String &json, Thermostat &info);
};
