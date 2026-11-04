#pragma once
#include <HTTPClient.h>
#include <Print.h>
#include "../models/thermostat.h"

class FritzThermostat
{
private:
  std::string static urlEncode(const char *msg);

public:
  void static printPostBody(Stream &stream, Thermostat &info, const char *sid);
  String static getPostBody(Thermostat &info, const char *sid);
  bool static parseDevices(HTTPClient &http, std::vector<Thermostat> &thermostats);
  bool static parseHolidayTemperature(Stream &stream, Thermostat &info);
};
