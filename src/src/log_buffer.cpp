#include "log_buffer.h"
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

void LogBuffer::add(unsigned long millis, char level, const char *message)
{
  if (!mData)
  {
    if (mFailed)
      return;
    mData = (char *)heap_caps_malloc(mCapacity, MALLOC_CAP_SPIRAM);
    if (!mData)
    {
      mFailed = true;
      return;
    }
  }

  char prefix[24];
  int prefixLength = snprintf(prefix, sizeof(prefix), "[%6lu][%c] ", millis, level);
  size_t messageLength = strlen(message);
  size_t lineLength = prefixLength + messageLength + 1;
  if (lineLength > mCapacity)
    return;
  while (mCapacity - mLength < lineLength)
    dropOldestLine();
  push(prefix, prefixLength);
  push(message, messageLength);
  push("\n", 1);
  mLines++;
  mNext++;
}

void LogBuffer::copyTo(char *out) const
{
  size_t first = std::min(mLength, mCapacity - mStart);
  memcpy(out, mData + mStart, first);
  memcpy(out + first, mData, mLength - first);
}

// Appends bytes after the newest ones; the caller made room for them
void LogBuffer::push(const char *bytes, size_t count)
{
  size_t end = (mStart + mLength) % mCapacity;
  size_t first = std::min(count, mCapacity - end);
  memcpy(mData + end, bytes, first);
  memcpy(mData, bytes + first, count - first);
  mLength += count;
}

void LogBuffer::dropOldestLine()
{
  while (mLength > 0)
  {
    char c = mData[mStart];
    mStart = (mStart + 1) % mCapacity;
    mLength--;
    if (c == '\n')
      break;
  }
  mLines--;
}
