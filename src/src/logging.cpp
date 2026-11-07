#include "logging.h"
#include "json.h"
#include "log_buffer.h"
#include <esp_heap_caps.h>
#include <mutex>
#include <stdarg.h>

// Bytes of log lines kept for /api/diagnostics: a few hundred lines, 2-3 minutes of a board that logs the advertisement
// of every sensor reading
#define LOG_BUFFER_SIZE (64 * 1024)

// Longest message, longer ones are cut. The JSON of a sensor advertisement takes up to about 300 bytes.
#define LOG_MESSAGE_SIZE 512

static LogBuffer logBuffer(LOG_BUFFER_SIZE);
static std::mutex logMutex;

void logLine(char level, const char *color, const char *format, ...)
{
  std::lock_guard<std::mutex> lock(logMutex);
  // static, as the tasks that log have small stacks; the lock guards it
  static char message[LOG_MESSAGE_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  // a kept line is one line
  for (char *c = message; *c; c++)
  {
    if (*c == '\n' || *c == '\r')
      *c = ' ';
  }

  unsigned long now = millis();
  Serial.printf("%s[%6lu] %s%s\r\n", color, now, message, LOG_COLOR_RESET);
  logBuffer.add(now, level, message);
}

void printLog(Print &out, uint32_t after)
{
  // Copy the lines, so that logging doesn't wait while they are sent
  char *lines = nullptr;
  size_t length = 0;
  uint32_t first, next, shown;
  {
    std::lock_guard<std::mutex> lock(logMutex);
    first = logBuffer.first();
    next = logBuffer.next();
    length = logBuffer.length();
    // an after beyond next comes from before a restart; lines older than the oldest kept one are gone
    shown = after > next ? first : after > first ? after : first;
    // no copy without new lines, as for most requests of a web UI that asks every few seconds
    if (length > 0 && shown < next)
    {
      lines = (char *)heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM);
      if (lines)
        logBuffer.copyTo(lines);
    }
  }

  out.printf("{\"first\":%lu,\"next\":%lu,\"lines\":[", (unsigned long)shown, (unsigned long)next);
  if (lines)
  {
    lines[length] = 0;
    char *line = lines;
    uint32_t skip = shown - first;
    for (bool firstShown = true; *line;)
    {
      char *end = strchr(line, '\n');
      if (end)
        *end = 0;
      if (skip > 0)
      {
        skip--;
      }
      else
      {
        if (!firstShown)
          out.print(',');
        printJsonString(out, line);
        firstShown = false;
      }
      if (!end)
        break;
      line = end + 1;
    }
    free(lines);
  }
  out.print("]}");
}
