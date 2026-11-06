#pragma once
#include "data_lua_api.h"
#include "rest_api.h"
#include <HTTPClient.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <WString.h>
#include <ctime>

// Login to the FRITZ!Box, and the API that reads the thermostats and writes their offset, chosen after each login
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

  DataLuaApi mDataLuaApi;
  ReadOnlyDataLuaApi mReadOnlyApi;
  RestApi mRestApi;
  // the API chosen after the login, NULL while not logged in
  ThermostatApi *mApi = nullptr;
  // FRITZ!OS version, e.g. "8.26"; empty if unknown
  std::string mVersion;

  bool query_challenge();
  bool query_session_id();
  bool detect_api();
  void query_version();
  int probe_rest_api();
  void begin_rest(HTTPClient &http, const char *path);
  void track_request(int httpCode);

public:
  Session(const char *host, int port, const char *username, const char *password);
  Session();
  ~Session();

  void configure(const char *host, int port, const char *username, const char *password);
  bool ensure_connection();
  void reset_session();
  std::string sid();
  // the API chosen after the login, NULL while not logged in
  ThermostatApi *api() const;
  // reads with sh_control, which reports the numeric device ids of the thermostats, and on 8.x their UIDs too
  DataLuaApi &dataLuaApi();
  const std::string &version() const;

  // HTTP requests of the APIs. They keep the session after a success, and end it after 401/403 or a failed connection.
  // POST a form; the caller reads the answer from http and ends it
  int post_form(HTTPClient &http, const char *path, const String &body);
  // GET from the REST API; the body is put in payload if the answer is 200
  int rest_get(const char *path, String &payload);
  // PUT a JSON body to the REST API
  int rest_put(const char *path, const String &body);
};
