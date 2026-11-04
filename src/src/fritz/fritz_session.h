#pragma once
#include "fritz_thermostat.h"
#include <HTTPClient.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <WString.h>
#include <ctime>

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

  bool query_challenge();
  bool query_session_id();

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
};