#include "crypto.h"
#include "fritz_session.h"
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
      mTimeTillLogout(1100),
      mDataLuaApi(*this),
      mReadOnlyApi(*this),
      mRestApi(*this)
{
}

Session::Session()
    : mUsername(""),
      mPassword(""),
      mHost("fritz.box"),
      mPort(80),
      mTimeTillLogout(1100),
      mDataLuaApi(*this),
      mReadOnlyApi(*this),
      mRestApi(*this)
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
  mApi = nullptr;
}

ThermostatApi *Session::api() const
{
  return mApi;
}

DataLuaApi &Session::dataLuaApi()
{
  return mDataLuaApi;
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

// Start a request to the REST API. It asks for HTTP/1.0, so that FRITZ!Box answers without chunked encoding:
// HTTPClient of the ESP32 core 2.0.x reads a chunked answer up to its last chunk, but not the empty line after it. Its
// writeToStream() (behind getString()) then closes the connection, and WiFiClient::flush() fails on that line, which
// is buffered, not in the socket ("flush(): fail on fd ..., errno: 11"). HTTPClient closes the connection after each
// request anyway (~HTTPClient()), so HTTP/1.0 costs nothing.
void Session::begin_rest(HTTPClient &http, const char *path)
{
  http.useHTTP10(true);
  http.begin(client, mHost.c_str(), mPort, path, false);
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
  begin_rest(http, path);
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

// Choose how to read the thermostats and write the offset. Runs after each login: a FRITZ!OS update reboots the
// FRITZ!Box, which forces a new login, so FritzGate switches the API without a restart.
bool Session::detect_api()
{
  query_version();
  int major = 0, minor = 0;
  bool versionKnown = sscanf(mVersion.c_str(), "%d.%d", &major, &minor) == 2;
  if (versionKnown && major < 8)
  {
    // FRITZ!OS 7.x has no REST API, no need to probe it
    mApi = &mDataLuaApi;
  }
  else
  {
    int rest = probe_rest_api();
    if (rest < 0)
      return false;
    if (rest > 0)
      mApi = &mRestApi;
    else if (versionKnown)
      mApi = &mReadOnlyApi; // FRITZ!OS 8.0x-8.1x, the REST API came with 8.20
    else
      mApi = &mDataLuaApi; // version unknown and no REST API: as before, treat it as 7.x
  }

  LOG_SUCCESSF("FRITZ!OS %s, API: %s", mVersion.empty() ? "unknown" : mVersion.c_str(), mApi->name());
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

// Keep the session after a successful request. End it after 401/403 or a failed connection, so that the next sync logs
// in again: FRITZ!OS 8.x can end a session early.
void Session::track_request(int httpCode)
{
  if (httpCode >= 200 && httpCode < 300)
    mSessionExpiration = std::time(nullptr) + mTimeTillLogout;
  else if (httpCode <= 0 || httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == HTTP_CODE_FORBIDDEN)
    reset_session();
}

int Session::post_form(HTTPClient &http, const char *path, const String &body)
{
  http.begin(client, mHost.c_str(), mPort, path, false);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(body);
  if (httpCode == HTTP_CODE_OK)
    LOG_HTTPF("HTTP POST %s: %d", path, httpCode);
  else if (httpCode > 0)
    LOG_ERRORF("HTTP POST %s: %d", path, httpCode);
  else
    LOG_ERRORF("HTTP POST failed: %s", http.errorToString(httpCode).c_str());
  track_request(httpCode);
  return httpCode;
}

int Session::rest_get(const char *path, String &payload)
{
  HTTPClient http;
  begin_rest(http, path);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    LOG_HTTPF("HTTP GET %s: %d", path, httpCode);
    // unlike getStream(), getString() also reads a chunked body
    payload = http.getString();
  }
  else if (httpCode > 0)
  {
    // the REST API explains errors: {"errors":[{"code":...,"field":...,"message":...}]}
    String body = http.getString();
    LOG_ERRORF("HTTP GET %s: %d %s", path, httpCode, body.substring(0, 300).c_str());
  }
  else
  {
    LOG_ERRORF("HTTP GET failed: %s", http.errorToString(httpCode).c_str());
  }
  http.end();
  track_request(httpCode);
  return httpCode;
}

int Session::rest_put(const char *path, const String &body)
{
  HTTPClient http;
  begin_rest(http, path);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PUT(body);
  if (httpCode >= 200 && httpCode < 300)
  {
    LOG_HTTPF("HTTP PUT %s: %d", path, httpCode);
    http.getString(); // read the answer, so that end() has nothing left to discard (see begin_rest())
  }
  else if (httpCode > 0)
  {
    String payload = http.getString();
    LOG_ERRORF("HTTP PUT %s: %d %s", path, httpCode, payload.substring(0, 300).c_str());
  }
  else
  {
    LOG_ERRORF("HTTP PUT failed: %s", http.errorToString(httpCode).c_str());
  }
  http.end();
  track_request(httpCode);
  return httpCode;
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
