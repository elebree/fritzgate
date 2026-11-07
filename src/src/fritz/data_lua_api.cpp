#include "data_lua_api.h"
#include "fritz_session.h"
#include "fritz_thermostat.h"
#include "../logging.h"
#include <HTTPClient.h>

bool DataLuaApi::queryThermostats(std::vector<Thermostat> &thermostats)
{
  LOG_INFO("FRITZ devices query");
  HTTPClient http;
  String body = String("xhr=1&sid=") + mSession.sid().c_str() + "&lang=en&page=sh_control&xhrId=all";
  bool result = mSession.post_form(http, "/data.lua", body) == HTTP_CODE_OK &&
                FritzThermostat::parseDevices(http, thermostats);
  http.end();
  return result;
}

// Read the holiday temperature, which is not part of the device JSON, from the thermostat edit form
bool DataLuaApi::queryHolidayTemperature(Thermostat &thermostat)
{
  LOG_INFOF("FRITZ holiday temperature query: %s", thermostat.id.c_str());
  HTTPClient http;
  http.setReuse(false); // the form is not read to the end, close the connection instead of reusing it
  String body = String("xhr=1&sid=") + mSession.sid().c_str() + "&device=" + thermostat.id.c_str() +
                "&page=home_auto_hkr_edit";
  bool found = mSession.post_form(http, "/data.lua", body) == HTTP_CODE_OK &&
               FritzThermostat::parseHolidayTemperature(http.getStream(), thermostat);
  http.end();
  return found;
}

bool DataLuaApi::setOffset(Thermostat &thermostat, int32_t offset)
{
  // posting enabled holidays without their temperature disables them, so rather keep the offset
  if (thermostat.enabledHolidays() > 0 && !queryHolidayTemperature(thermostat))
  {
    logSkip(thermostat, offset, "holiday temperature unknown");
    return false;
  }
  logUpdate(thermostat, offset);

  // the form posts all settings: the ones read from the FRITZ!Box, with the new offset
  Thermostat changed = thermostat;
  changed.offset = offset;
  String body = FritzThermostat::getPostBody(changed, mSession.sid().c_str());
  // log the form without the session ID
  LOG_HTTP(FritzThermostat::getPostBody(changed, "[SID]").c_str());

  HTTPClient http;
  int httpCode = mSession.post_form(http, "/data.lua", body);
  if (httpCode == HTTP_CODE_OK)
    http.getString();
  http.end();
  // FRITZ!Box answers 200 also when it rejects the form
  return httpCode == HTTP_CODE_OK;
}

bool ReadOnlyDataLuaApi::setOffset(Thermostat &thermostat, int32_t offset)
{
  logSkip(thermostat, offset, "FRITZ!OS without Smart Home REST API");
  return false;
}
