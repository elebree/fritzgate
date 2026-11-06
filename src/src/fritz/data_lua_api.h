#pragma once
#include "thermostat_api.h"

// FRITZ!OS 7.x: reads the thermostats with sh_control and writes the offset with the thermostat edit form
// (data.lua, page=home_auto_hkr_edit), which posts all settings of the thermostat. The thermostat id is the numeric
// device id.
class DataLuaApi : public ThermostatApi
{
public:
  using ThermostatApi::ThermostatApi;

  const char *name() const override { return "data.lua"; }
  bool usesUids() const override { return false; }
  bool queryThermostats(std::vector<Thermostat> &thermostats) override;
  bool setOffset(Thermostat &thermostat, int32_t offset) override;

private:
  bool queryHolidayTemperature(Thermostat &thermostat);
};

// FRITZ!OS 8.0x-8.1x: reads like 7.x, but writes nothing. It has no REST API, and its edit form differs from 7.x:
// posting the 7.x form could reset other settings.
class ReadOnlyDataLuaApi : public DataLuaApi
{
public:
  using DataLuaApi::DataLuaApi;

  const char *name() const override { return "data.lua (read-only)"; }
  bool setOffset(Thermostat &thermostat, int32_t offset) override;
};
