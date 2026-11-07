#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * The last log lines, for /api/diagnostics: a ring of bytes in PSRAM, allocated with the first line. Each line is kept
 * as "[   1234][W] message\n"; when the ring is full, the oldest lines make room. The lines are numbered from the start,
 * so that the web UI can ask for the ones after the last it has. Not thread-safe: logLine() locks it.
 * Like History, it needs only heap_caps_malloc, so it can be compiled on a PC against stubs.
 */
class LogBuffer
{
public:
  // constexpr, so that a global one is ready before any constructor logs
  constexpr explicit LogBuffer(size_t capacity) : mCapacity(capacity) {}

  // Keeps a line of the given milliseconds since the start and level letter. The message must not contain '\n'.
  void add(unsigned long millis, char level, const char *message);
  // Bytes of the kept lines
  size_t length() const { return mLength; }
  // Number of the oldest kept line, and of the next line
  uint32_t first() const { return mNext - mLines; }
  uint32_t next() const { return mNext; }
  // Copies the kept lines, oldest first, to out, which has room for length() bytes
  void copyTo(char *out) const;

private:
  size_t mCapacity;
  char *mData = nullptr;
  // the allocation failed, so no line is kept
  bool mFailed = false;
  // offset of the oldest byte, and bytes kept from there, wrapping at mCapacity
  size_t mStart = 0;
  size_t mLength = 0;
  // lines kept, and lines kept since the start
  uint32_t mLines = 0;
  uint32_t mNext = 0;

  void push(const char *bytes, size_t count);
  void dropOldestLine();
};
