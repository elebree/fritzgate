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
  mApi = FRITZ_API_UNKNOWN;
}

FritzApi Session::api() const
{
  return mApi;
}

const std::string &Session::version() const
{
  return mVersion;
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

  if (!detect_api())
  {
    LOG_ERROR("FRITZ!Box API detection failed");
    reset_session();
    return false;
  }

  mSessionExpiration = std::time(nullptr) + mTimeTillLogout;
  return true;
}

// Keeps the first non-whitespace character of a response body and drops the rest, so a large body needs no memory
class FirstCharStream : public Stream
{
public:
  char first = 0;

  size_t write(uint8_t c) override
  {
    if (first == 0 && !isspace(c))
      first = c;
    return 1;
  }

  size_t write(const uint8_t *buffer, size_t size) override
  {
    for (size_t i = 0; i < size && first == 0; i++)
      write(buffer[i]);
    return size;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

void Session::add_rest_auth_header(HTTPClient &http)
{
  http.addHeader("Authorization", String("AVM-SID ") + mSID.c_str());
}

// Read the FRITZ!OS version, e.g. "8.26", from jason_boxinfo.xml, which needs no login
void Session::query_version()
{
  mVersion = "";
  HTTPClient http;
  http.begin(client, mHost.c_str(), mPort, "/jason_boxinfo.xml", false);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    // <j:Version>271.08.26</j:Version>: hardware id, major and minor version
    String payload = http.getString();
    std::string version = getTagValue(payload.c_str(), "<j:Version>", "</j:Version>");
    int major, minor;
    if (sscanf(version.c_str(), "%*d.%d.%d", &major, &minor) == 2)
    {
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "%d.%02d", major, minor);
      mVersion = buffer;
    }
  }
  else
  {
    LOG_WARNINGF("HTTP GET /jason_boxinfo.xml: %d", httpCode);
  }
  http.end();
}

// 1 if the Smart Home REST API (FRITZ!OS 8.20+) answers, 0 if not, -1 if the request failed
int Session::probe_rest_api()
{
  const char *path = "/api/v0/smarthome/overview/units";
  HTTPClient http;
  http.begin(client, mHost.c_str(), mPort, path, false);
  add_rest_auth_header(http);
  int httpCode = http.GET();
  int result = 0;
  if (httpCode == HTTP_CODE_OK)
  {
    // check the body too, an unknown path may be answered with an HTML page
    FirstCharStream body;
    http.writeToStream(&body);
    result = body.first == '[' ? 1 : 0;
  }
  else if (httpCode <= 0)
  {
    LOG_ERRORF("HTTP GET failed: %s", http.errorToString(httpCode).c_str());
    result = -1;
  }
  LOG_HTTPF("HTTP GET %s: %d", path, httpCode);
  http.end();
  return result;
}

// Choose how to write the offset. Runs after each login: a FRITZ!OS update reboots the FRITZ!Box, which forces a new
// login, so FritzGate switches the API without a restart.
bool Session::detect_api()
{
  query_version();
  int major = 0, minor = 0;
  bool versionKnown = sscanf(mVersion.c_str(), "%d.%d", &major, &minor) == 2;
  if (versionKnown && major < 8)
  {
    // FRITZ!OS 7.x has no REST API, no need to probe it
    mApi = FRITZ_API_DATA_LUA;
  }
  else
  {
    int rest = probe_rest_api();
    if (rest < 0)
      return false;
    if (rest > 0)
      mApi = FRITZ_API_REST;
    else if (versionKnown)
      mApi = FRITZ_API_READ_ONLY; // FRITZ!OS 8.0x-8.1x, the REST API came with 8.20
    else
      mApi = FRITZ_API_DATA_LUA; // version unknown and no REST API: as before, treat it as 7.x
  }

  const char *apiName = mApi == FRITZ_API_REST       ? "Smart Home REST API"
                        : mApi == FRITZ_API_DATA_LUA ? "data.lua form"
                                                     : "none (read-only)";
  LOG_SUCCESSF("FRITZ!OS %s, offset written with: %s", mVersion.empty() ? "unknown" : mVersion.c_str(), apiName);
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
    else if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == HTTP_CODE_FORBIDDEN)
    {
      // FRITZ!OS 8.x can end a session early, log in again with the next sync
      reset_session();
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

// Read the holiday temperature, which is not part of the device JSON, from the thermostat edit form
bool Session::query_holiday_temperature(Thermostat &thermostat)
{
  HTTPClient http;
  http.setReuse(false); // the form is not read to the end, close the connection instead of reusing it
  http.begin(client, mHost.c_str(), mPort, "/data.lua", false);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = String("xhr=1&sid=") + mSID.c_str() + "&device=" + thermostat.id.c_str() + "&page=home_auto_hkr_edit";
  int httpCode = http.POST(body);
  bool found = false;
  if (httpCode == HTTP_CODE_OK)
  {
    LOG_HTTPF("HTTP POST /data.lua home_auto_hkr_edit: %d", httpCode);
    found = FritzThermostat::parseHolidayTemperature(http.getStream(), thermostat);
  }
  else if (httpCode > 0)
  {
    LOG_ERRORF("HTTP POST /data.lua home_auto_hkr_edit: %d", httpCode);
  }
  else
  {
    reset_session();
    LOG_ERRORF("HTTP POST failed: %s", http.errorToString(httpCode).c_str());
  }
  http.end();
  return found;
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

// Set only the offset with the Smart Home REST API (FRITZ!OS 8.20+); true if FRITZ!Box accepted it
bool Session::put_thermostat_offset(Thermostat &thermostat, int32_t offset)
{
  std::string path = FritzThermostat::getOffsetPath(thermostat);
  String body = FritzThermostat::getOffsetBody(offset);
  LOG_HTTPF("HTTP PUT %s %s", path.c_str(), body.c_str());

  HTTPClient http;
  http.begin(client, mHost.c_str(), mPort, path.c_str(), false);
  add_rest_auth_header(http);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PUT(body);
  bool accepted = httpCode >= 200 && httpCode < 300;
  if (accepted)
  {
    mSessionExpiration = std::time(nullptr) + mTimeTillLogout;
    LOG_HTTPF("HTTP PUT %s: %d", path.c_str(), httpCode);
  }
  else if (httpCode > 0)
  {
    // the REST API explains errors: {"errors":[{"code":...,"field":...,"message":...}]}
    String payload = http.getString();
    LOG_ERRORF("HTTP PUT %s: %d %s", path.c_str(), httpCode, payload.substring(0, 300).c_str());
    if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == HTTP_CODE_FORBIDDEN)
      reset_session(); // log in again with the next sync
  }
  else
  {
    reset_session();
    LOG_ERRORF("HTTP PUT failed: %s", http.errorToString(httpCode).c_str());
  }
  http.end();
  return accepted;
}
