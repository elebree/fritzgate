#include "thermostat_api.h"
#include "../logging.h"

void ThermostatApi::logUpdate(const Thermostat &thermostat, int32_t offset)
{
  LOG_INFOF("Updating offset for \"%s\": %g -> %g", thermostat.name.c_str(), thermostat.offset / 10.0, offset / 10.0);
  if (thermostat.usesExternalSensor())
    LOG_INFOF("\"%s\": the reference sensor replaces the external sensor set in FRITZ!Box", thermostat.name.c_str());
}

void ThermostatApi::logSkip(const Thermostat &thermostat, int32_t offset, const char *reason)
{
  LOG_WARNINGF("Skip offset for \"%s\" %g -> %g: %s",
               thermostat.name.c_str(),
               thermostat.offset / 10.0,
               offset / 10.0,
               reason);
}
