#pragma once
#include <Print.h>
#include <string>

// Writes a JSON string literal. Quotes, backslashes and control characters are escaped, other bytes (UTF-8) are
// written as they are. Runs without escapes are written at once, not byte by byte.
inline void printJsonString(Print &out, const char *value)
{
  out.print('"');
  const char *run = value;
  for (const char *p = value; *p; p++)
  {
    unsigned char c = (unsigned char)*p;
    if (c != '"' && c != '\\' && c >= 0x20)
      continue;
    out.write((const uint8_t *)run, p - run);
    if (c == '"' || c == '\\')
    {
      out.print('\\');
      out.print((char)c);
    }
    else
    {
      out.printf("\\u%04x", c);
    }
    run = p + 1;
  }
  out.print(run);
  out.print('"');
}

// Writes "key": "value", with the value escaped
inline void printJsonMember(Print &out, const char *key, const char *value)
{
  out.printf("\"%s\": ", key);
  printJsonString(out, value);
}

inline void printJsonMember(Print &out, const char *key, const std::string &value)
{
  printJsonMember(out, key, value.c_str());
}
