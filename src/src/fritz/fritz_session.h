#pragma once
#include "fritz_thermostat.h"
#include <HTTPClient.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <WString.h>
#include <ctime>

// How FritzGate writes the thermostat offset, detected after each login
enum FritzApi
{
  FRITZ_API_UNKNOWN,   // not logged in yet
  FRITZ_API_DATA_LUA,  // FRITZ!OS 7.x: the thermostat edit form (data.lua, page=home_auto_hkr_edit)
  FRITZ_API_REST,      // FRITZ!OS 8.20+: Smart Home REST API (/api/v0/smarthome/...)
  FRITZ_API_READ_ONLY, // FRITZ!OS 8.x without the REST API: its edit form differs from 7.x, so don't post it
};

class Session
{
private:
  const char *loginPath = "/login_sid.lua?version=2";

  std::string mHost;
  int mPort;
  std::string mUsername;
  std::string mPassword;

  std::string mChallenge;
  std::string mSID;
  std::time_t mSessionExpiration;
  long mTimeTillLogout;

  FritzApi mApi = FRITZ_API_UNKNOWN;
  // FRITZ!OS version, e.g. "8.26"; empty if unknown
  std::string mVersion;

  bool query_challenge();
  bool query_session_id();
  bool detect_api();
  void query_version();
  int probe_rest_api();
  void add_rest_auth_header(HTTPClient &http);

public:
  Session(const char *host, int port, const char *username, const char *password);
  Session();
  ~Session();

  void configure(const char *host, int port, const char *username, const char *password);
  bool ensure_connection();
  void reset_session();
  std::string sid();
  int post_request(HTTPClient &http, const char *path, const char *post_template);
  String query_data(const char *path, const char *post_template);
  bool query_devices(std::vector<Thermostat> &thermostats);
  bool query_holiday_temperature(Thermostat &thermostat);
  bool post_thermostat_state(Thermostat &thermostat);
  bool put_thermostat_offset(Thermostat &thermostat, int32_t offset);
  FritzApi api() const;
  const std::string &version() const;
};