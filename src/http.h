#pragma once
#include <Stream.h>

static const char HeaderAccessControlAllowHeaders[] PROGMEM = "Access-Control-Allow-Headers";
static const char HeaderAccessControlAllowMethods[] PROGMEM = "Access-Control-Allow-Methods";
static const char HeaderAccessControlAllowOrigin[] PROGMEM = "Access-Control-Allow-Origin";
static const char HeaderConnection[] PROGMEM = "Connection";
static const char HeaderContentEncoding[] PROGMEM = "Content-Encoding";
static const char HeaderContentLength[] PROGMEM = "Content-Length";
static const char HeaderContentType[] PROGMEM = "Content-Type";
static const char HeaderRefresh[] PROGMEM = "Refresh";

static const char CONTENT_TYPE_JSON[] PROGMEM = "application/json";

void writeHttpHeader(Stream &client, const char *header, const char *value)
{
  client.printf("%s: %s\r\n", header, value);
}

void writeHttpStatus(Stream &client, int status = 200, const char *message = "OK")
{
  client.printf("HTTP/1.1 %d %s\r\n", status, message);
  writeHttpHeader(client, HeaderConnection, "close"); // the connection will be closed after completion of the response
}

void writeJsonHeaders(Stream &client, int status = 200, const char *message = "OK")
{
  writeHttpStatus(client, status, message);
  writeHttpHeader(client, HeaderAccessControlAllowOrigin, "*");
  writeHttpHeader(client, HeaderAccessControlAllowMethods, "POST, GET, OPTIONS");
  writeHttpHeader(client, HeaderAccessControlAllowHeaders, HeaderContentType);
  writeHttpHeader(client, HeaderContentType, CONTENT_TYPE_JSON);
  client.println();
}