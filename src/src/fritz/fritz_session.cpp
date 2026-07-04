#include "crypto.h"
#include "fritz_session.h"
#include "fritz_thermostat.h"
#include "../logging.h"
#include <HTTPClient.h>
#include <vector>
#include <WiFi.h>

WiFiClient client;

std::string getTagValue(const char *payload, const char *startTag, const char *endTag)
{
  char *startPos = strstr(payload, startTag);
  if (startPos == NULL)
  {
    LOG_ERRORF("Missing tag: %s", startTag);
    return "";
  }
  startPos += strlen(startTag);
  char *stopPos = strstr(startPos, endTag);
  if (stopPos == NULL)
  {
    LOG_ERRORF("Missing closing tag: %s", endTag);
    return "";
  }
  return std::string(startPos, stopPos);
}

// FritzGate Session Constructor
Session::Session(const char *host, int port, const char *username, const char *password)
    : mUsername(username),
      mPassword(password),
      mHost(host),
      mPort(port),
      mTimeTillLogout(1100)
{
}

Session::Session()
    : mUsername(""),
      mPassword(""),
      mHost("fritz.box"),
      mPort(80),
      mTimeTillLogout(1100)
{
}

// Destructor for FritzGate Session
Session::~Session()
{
}

// Get Session ID
std::string Session::sid()
{
  return mSID;
}

void Session::reset_session()
{
  mSID = "";
  mSessionExpiration = 0;
}

void Session::configure(const char *host, int port, const char *username, const char *password)
{
  mHost = host;
  mPort = port;
  mUsername = username;
  mPassword = password;
  reset_session();
}

// Establish connection to FritzBox
bool Session::ensure_connection()
{
  if (mHost == "" || mUsername == "" || mPassword == "")
  {
    LOG_ERROR("FRITZ!Box credentials missing");
    return false;
  }

  if (mSID != "" && mSessionExpiration > std::time(nullptr))
  {
    return true;
  }

  if (!query_challenge())
  {
    LOG_ERROR("FRITZ!Box challenge failed");
    return false;
  }

  if (!query_session_id())
  {
    LOG_ERROR("FRITZ!Box session failed");
    return false;
  }

  mSessionExpiration = std::time(nullptr) + mTimeTillLogout;
  return true;
}

// Query Challenge for authentication
bool Session::query_challenge()
{
  HTTPClient http;
  http.begin(client, mHost.c_str(), mPort, loginPath, false); // Initiate HTTP connection
  int httpCode = http.GET();
  String payload;
  // Check if HTTP request was successful
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    LOG_HTTPF("HTTP GET %s: %d", loginPath, httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      payload = http.getString();
    }
  }
  else
  {
    LOG_ERRORF("HTTP GET failed: %s", http.errorToString(httpCode).c_str());
    return false;
  }

  http.end();

  mChallenge = getTagValue(payload.c_str(), "<Challenge>", "</Challenge>");
  return true;
}

// Perform POST request with provided HTTPClient and path
int Session::post_request(HTTPClient &http, const char *path, const char *post_template)
{
  char post_data[70];
  snprintf(post_data, sizeof(post_data), post_template, mSID.c_str());

  http.begin(client, mHost.c_str(), mPort, path, false); // Initiate HTTP connection
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST((uint8_t *)post_data, strlen(post_data));
  LOG_HTTP("HTTP POST body ready");
  String payload;
  if (httpCode > 0)
  {
    // HTTP header has been sent and Server response header has been handled
    if (httpCode == HTTP_CODE_OK)
    {
      // update session after successful request
      mSessionExpiration = std::time(nullptr) + mTimeTillLogout;
    }
    LOG_HTTPF("HTTP POST %s: %d", path, httpCode);
  }
  else
  {
    reset_session();
    LOG_ERRORF("HTTP POST failed: %s", http.errorToString(httpCode).c_str());
  }
  return httpCode;
}

// Query data with POST request
String Session::query_data(const char *path, const char *post_template)
{
  HTTPClient http;
  String payload;
  auto httpCode = post_request(http, path, post_template);
  // Check if HTTP request was successful
  if (httpCode == HTTP_CODE_OK)
  {
    payload = http.getString();
  }
  http.end();
  return payload;
}

// Query devices and populate vector with thermostats
bool Session::query_devices(std::vector<Thermostat> &thermostats)
{
  LOG_INFO("FRITZ devices query");
  HTTPClient http;
  // auto httpCode = post_request(http, "/myfritz/api/data.lua", "sid=%s&c=smarthome&a=getDevicesAndGroups");
  auto httpCode = post_request(http, "/data.lua", "xhr=1&sid=%s&lang=en&page=sh_control&xhrId=all");
  // Check if HTTP request was successful
  if (httpCode == HTTP_CODE_OK)
  {
    auto stat = FritzThermostat::parseDevices(http, thermostats);
    http.end();
    return stat;
  }
  http.end();
  return false;
}

// Query Session ID for authentication
bool Session::query_session_id()
{
  LOG_INFO("FRITZ session query");
  std::string response = calculate_pbkdf2_response(mChallenge.c_str(), mPassword.c_str());

  // Set POST Data
  std::string post_data = "username=" + mUsername + "&response=" + response;
  HTTPClient http;
  http.begin(client, mHost.c_str(), mPort, loginPath, false); // Initiate HTTP connection
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(String(post_data.c_str()));
  String payload;

  if (httpCode > 0)
  {
    // HTTP header has been sent and Server response header has been handled
    LOG_HTTPF("HTTP POST %s: %d", loginPath, httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      payload = http.getString();
    }
  }
  else
  {
    LOG_ERRORF("HTTP POST failed: %s", http.errorToString(httpCode).c_str());
    return false;
  }
  http.end();

  mSID = getTagValue(payload.c_str(), "<SID>", "</SID>");
  if (mSID.empty())
  {
    LOG_ERROR("FRITZ SID missing");
    return false;
  }

  if (mSID == "0000000000000000")
  {
    LOG_ERROR("FRITZ credentials rejected");
    return false;
  }

  LOG_SUCCESS("FRITZ authenticated");
  return true;
}

// Post thermostat state
bool Session::post_thermostat_state(Thermostat &thermostat)
{
  HTTPClient http;
  http.begin(client, mHost.c_str(), mPort, "/data.lua", false); // Initiate HTTP connection
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postBody = FritzThermostat::getPostBody(thermostat, mSID.c_str());
  int httpCode = http.POST(postBody.c_str());
  String payload;

  if (httpCode > 0)
  {
    // HTTP header has been sent and Server response header has been handled
    LOG_HTTPF("HTTP POST /data.lua: %d", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      payload = http.getString();
      // Serial.println(payload.c_str());
    }
  }
  else
  {
    LOG_ERRORF("HTTP POST failed: %s", http.errorToString(httpCode).c_str());
    return false;
  }
  http.end();
  return true;
}
