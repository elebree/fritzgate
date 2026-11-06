#include "rest_api.h"
#include "fritz_session.h"
#include "fritz_thermostat.h"
#include "../logging.h"
#include <HTTPClient.h>

bool RestApi::queryThermostats(std::vector<Thermostat> &thermostats)
{
  LOG_INFO("FRITZ devices query (REST)");
  // a thermostat gets an offset only from what FRITZ!Box reports now, not from an earlier reading
  for (Thermostat &thermostat : thermostats)
    thermostat.temperatureValid = false;

  String payload;
  std::vector<std::string> uids;
  if (mSession.rest_get("/api/v0/smarthome/overview/units", payload) != HTTP_CODE_OK ||
      !FritzThermostat::parseRestUnits(payload, thermostats, uids))
    return false;

  // model, AIN and battery belong to the device; the offset works without them
  if (mSession.rest_get("/api/v0/smarthome/overview/devices", payload) == HTTP_CODE_OK)
    FritzThermostat::parseRestDevices(payload, thermostats);

  // the offset is only part of the unit configuration; a thermostat whose configuration can't be read gets no offset
  for (const std::string &uid : uids)
  {
    if (mSession.api() != this)
      break; // the session ended
    for (Thermostat &thermostat : thermostats)
    {
      if (thermostat.id != uid)
        continue;
      std::string path = FritzThermostat::getUnitConfigurationPath(thermostat);
      if (mSession.rest_get(path.c_str(), payload) == HTTP_CODE_OK)
        FritzThermostat::parseRestUnitConfiguration(payload, thermostat);
      break;
    }
  }
  return true;
}

bool RestApi::setOffset(Thermostat &thermostat, int32_t offset)
{
  logUpdate(thermostat, offset);
  std::string path = FritzThermostat::getUnitConfigurationPath(thermostat);
  String body = FritzThermostat::getOffsetBody(offset);
  LOG_HTTPF("HTTP PUT %s %s", path.c_str(), body.c_str());
  int httpCode = mSession.rest_put(path.c_str(), body);
  return httpCode >= 200 && httpCode < 300;
}
