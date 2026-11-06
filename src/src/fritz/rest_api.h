#pragma once
#include "thermostat_api.h"

// FRITZ!OS 8.20+: the Smart Home REST API (/api/v0/smarthome/...). It writes only the offset, and identifies the
// thermostats by their unit UID, as it has no numeric ids.
class RestApi : public ThermostatApi
{
public:
  using ThermostatApi::ThermostatApi;

  const char *name() const override { return "Smart Home REST API"; }
  bool usesUids() const override { return true; }
  bool queryThermostats(std::vector<Thermostat> &thermostats) override;
  bool setOffset(Thermostat &thermostat, int32_t offset) override;
};
