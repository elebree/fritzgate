#pragma once
#include "../models/thermostat.h"
#include <vector>

class Session;

// Reads the thermostats from the FRITZ!Box and writes their offset: one implementation per FRITZ!OS API generation.
// Session chooses one after each login (Session::api()) and provides the login and the HTTP requests.
class ThermostatApi
{
public:
  explicit ThermostatApi(Session &session) : mSession(session) {}
  virtual ~ThermostatApi() {}

  // for the log, e.g. "Smart Home REST API"
  virtual const char *name() const = 0;
  // whether the thermostat id is the unit UID; otherwise it is the numeric device id of sh_control
  virtual bool usesUids() const = 0;
  // Read the thermostats: update the known ones and add new ones. True if the FRITZ!Box answered the list of
  // thermostats, also an empty one; a thermostat whose state could not be read keeps temperatureValid false.
  virtual bool queryThermostats(std::vector<Thermostat> &thermostats) = 0;
  // Write the offset in 0.1 °C, or log why it can't be written now. True if the FRITZ!Box took it; setting
  // thermostat.offset is left to the caller.
  virtual bool setOffset(Thermostat &thermostat, int32_t offset) = 0;

protected:
  Session &mSession;

  void logUpdate(const Thermostat &thermostat, int32_t offset);
  void logSkip(const Thermostat &thermostat, int32_t offset, const char *reason);
};
