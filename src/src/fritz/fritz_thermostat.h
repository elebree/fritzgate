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
  // Smart Home REST API (FRITZ!OS 8.20+): path and body of the PUT that sets only the offset
  std::string static getOffsetPath(const Thermostat &info);
  String static getOffsetBody(int32_t offset);
  bool static parseDevices(HTTPClient &http, std::vector<Thermostat> &thermostats);
  bool static parseHolidayTemperature(Stream &stream, Thermostat &info);
};
