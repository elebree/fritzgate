#pragma once

#include <Arduino.h>

#define LOG_COLOR_RESET "\x1b[0m"
#define LOG_COLOR_INFO "\x1b[36m"
#define LOG_COLOR_SUCCESS "\x1b[32m"
#define LOG_COLOR_WARNING "\x1b[33m"
#define LOG_COLOR_ERROR "\x1b[31m"
#define LOG_COLOR_HTTP "\x1b[34m"
#define LOG_COLOR_DEVICE "\x1b[35m"

#define LOG_BEGIN(color) Serial.print(color)
#define LOG_END() Serial.print(LOG_COLOR_RESET)
#define LOG_ENDLN() Serial.println(LOG_COLOR_RESET)

#define LOG_INLINE(color, message) \
  do                               \
  {                                \
    Serial.print(color);           \
    Serial.print(message);         \
    Serial.print(LOG_COLOR_RESET); \
  } while (0)

#define LOG_LINE(color, message)    \
  do                                \
  {                                 \
    Serial.print(color);            \
    Serial.print(message);          \
    Serial.println(LOG_COLOR_RESET); \
  } while (0)

#define LOG_LINEF(color, format, ...) \
  do                                  \
  {                                   \
    Serial.print(color);              \
    Serial.printf(format, __VA_ARGS__); \
    Serial.println(LOG_COLOR_RESET);  \
  } while (0)

#define LOG_INFO(message) LOG_LINE(LOG_COLOR_INFO, message)
#define LOG_SUCCESS(message) LOG_LINE(LOG_COLOR_SUCCESS, message)
#define LOG_WARNING(message) LOG_LINE(LOG_COLOR_WARNING, message)
#define LOG_ERROR(message) LOG_LINE(LOG_COLOR_ERROR, message)
#define LOG_HTTP(message) LOG_LINE(LOG_COLOR_HTTP, message)
#define LOG_DEVICE(message) LOG_LINE(LOG_COLOR_DEVICE, message)
#define LOG_HTTPF(format, ...) LOG_LINEF(LOG_COLOR_HTTP, format, __VA_ARGS__)
#define LOG_INFOF(format, ...) LOG_LINEF(LOG_COLOR_INFO, format, __VA_ARGS__)
#define LOG_SUCCESSF(format, ...) LOG_LINEF(LOG_COLOR_SUCCESS, format, __VA_ARGS__)
#define LOG_WARNINGF(format, ...) LOG_LINEF(LOG_COLOR_WARNING, format, __VA_ARGS__)
#define LOG_ERRORF(format, ...) LOG_LINEF(LOG_COLOR_ERROR, format, __VA_ARGS__)
#define LOG_DEVICEF(format, ...) LOG_LINEF(LOG_COLOR_DEVICE, format, __VA_ARGS__)
