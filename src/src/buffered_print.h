#pragma once
#include <Print.h>
#include <string.h>
#include <algorithm>

/**
 * Collects small writes into chunks of up to N bytes. Each write to a WiFiClient is sent on its own, which makes a long
 * answer printed number by number slow. Call flush() at the end.
 */
template <size_t N>
class BufferedPrint : public Print
{
public:
  explicit BufferedPrint(Print &out) : out(out) {}

  size_t write(uint8_t c) override
  {
    if (length == N)
      flush();
    buffer[length++] = c;
    return 1;
  }

  size_t write(const uint8_t *data, size_t size) override
  {
    size_t left = size;
    while (left > 0)
    {
      if (length == N)
        flush();
      size_t chunk = std::min(left, N - length);
      memcpy(buffer + length, data, chunk);
      length += chunk;
      data += chunk;
      left -= chunk;
    }
    return size;
  }

  void flush() override
  {
    if (length > 0)
      out.write(buffer, length);
    length = 0;
  }

private:
  Print &out;
  uint8_t buffer[N];
  size_t length = 0;
};
