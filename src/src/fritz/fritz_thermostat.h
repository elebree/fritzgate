#pragma once
#include <HTTPClient.h>
#include <Print.h>
#include "../models/thermostat.h"

class FritzThermostat
{
public:
  std::string static urlEncode(const char *msg, bool spaceAsPlus = true);
  // Body of the thermostat edit form post. fritzOs8: the legacy form of FRITZ!OS 8.x (DataLua8Api), else 7.x.
  void static printPostBody(Stream &stream, Thermostat &info, const char *sid, bool fritzOs8 = false);
  String static getPostBody(Thermostat &info, const char *sid, bool fritzOs8 = false);
  // Smart Home REST API (FRITZ!OS 8.20+): configuration path of the unit, read with GET and changed with PUT
  std::string static getUnitConfigurationPath(const Thermostat &info);
  // Smart Home REST API: body of the PUT that sets only the offset
  String static getOffsetBody(int32_t offset);
  // useUids: the thermostat id is the unit UID (the AIN), as with the REST API; otherwise the numeric device id
  bool static parseDevices(HTTPClient &http, std::vector<Thermostat> &thermostats, bool useUids = false);
  bool static parseHolidayTemperature(Stream &stream, Thermostat &info);
  // The answer to an edit form post: true if FRITZ!Box took the form ("apply": "ok"). It answers 200 also when it
  // rejects the form.
  bool static parseApplyAnswer(Stream &stream);
  // The answer to saveTemperature (page=sh_control): true if FRITZ!Box answered it. changed: whether it set the target
  // temperature ("done": true); "done" is false when the target temperature already had that value.
  bool static parseSaveTemperatureAnswer(Stream &stream, bool &changed);
  // Smart Home REST API: add the thermostat units of GET overview/units to thermostats, and their UIDs to uids
  bool static parseRestUnits(const String &json, std::vector<Thermostat> &thermostats, std::vector<std::string> &uids);
  // Smart Home REST API: model, AIN and battery of the thermostats from GET overview/devices
  bool static parseRestDevices(const String &json, std::vector<Thermostat> &thermostats);
  // Smart Home REST API: state of a thermostat from GET configuration/units/{UID}
  bool static parseRestUnitConfiguration(const String &json, Thermostat &info);
};
