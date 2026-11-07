#pragma once

#include <Arduino.h>

#define LOG_COLOR_RESET "\x1b[0m"
#define LOG_COLOR_INFO "\x1b[36m"
#define LOG_COLOR_SUCCESS "\x1b[32m"
#define LOG_COLOR_WARNING "\x1b[33m"
#define LOG_COLOR_ERROR "\x1b[31m"
#define LOG_COLOR_HTTP "\x1b[34m"
#define LOG_COLOR_DEVICE "\x1b[35m"

// Writes a line to the serial port, in color, after the milliseconds since the start like the lines the ESP32 core logs
// ("[1230113][E]..."), and keeps it for /api/diagnostics (see printLogLines()), as "[   1234][W] message". The level
// is a letter: E(rror), W(arning), I(nfo), S(uccess), H(TTP), D(evice). Each line is written at once, so that lines of
// different tasks don't mix: the BLE scanner logs from its own task.
void logLine(char level, const char *color, const char *format, ...) __attribute__((format(printf, 3, 4)));

// Writes the kept lines numbered after or later, oldest first, as JSON: {"first": number of the first line written,
// "next": number to ask for the lines after these, "lines": [...]}. The lines are numbered from the start, so an after
// beyond next comes from before a restart: then all kept lines are written. The last LOG_BUFFER_SIZE bytes of lines
// are kept, in PSRAM; a restart loses them.
void printLog(Print &out, uint32_t after);

// Progress on the serial port only, not kept for /api/diagnostics, such as the dots while Wi-Fi connects:
// LOG_BEGIN() starts a line, LOG_ENDLN() ends it
#define LOG_BEGIN(color)                   \
  do                                       \
  {                                        \
    Serial.print(color);                   \
    Serial.printf("[%6lu] ", millis());    \
  } while (0)
#define LOG_ENDLN() Serial.println(LOG_COLOR_RESET)

#define LOG_INFO(message) logLine('I', LOG_COLOR_INFO, "%s", message)
#define LOG_SUCCESS(message) logLine('S', LOG_COLOR_SUCCESS, "%s", message)
#define LOG_WARNING(message) logLine('W', LOG_COLOR_WARNING, "%s", message)
#define LOG_ERROR(message) logLine('E', LOG_COLOR_ERROR, "%s", message)
#define LOG_HTTP(message) logLine('H', LOG_COLOR_HTTP, "%s", message)
#define LOG_DEVICE(message) logLine('D', LOG_COLOR_DEVICE, "%s", message)
#define LOG_HTTPF(format, ...) logLine('H', LOG_COLOR_HTTP, format, __VA_ARGS__)
#define LOG_INFOF(format, ...) logLine('I', LOG_COLOR_INFO, format, __VA_ARGS__)
#define LOG_SUCCESSF(format, ...) logLine('S', LOG_COLOR_SUCCESS, format, __VA_ARGS__)
#define LOG_WARNINGF(format, ...) logLine('W', LOG_COLOR_WARNING, format, __VA_ARGS__)
#define LOG_ERRORF(format, ...) logLine('E', LOG_COLOR_ERROR, format, __VA_ARGS__)
#define LOG_DEVICEF(format, ...) logLine('D', LOG_COLOR_DEVICE, format, __VA_ARGS__)
