// Copy src into dst as the body of a JSON string: escapes " and \, drops control bytes. High bytes
// (UTF-8) are dropped too unless keepUtf8 is set: LoRa payloads are untrusted and shown as ASCII, but
// Wi-Fi network names may legitimately be UTF-8.
#pragma once
#include <Arduino.h>

inline void jsonEscape(char* dst, size_t dstSize, const char* src, bool keepUtf8 = false) {
  size_t o = 0;
  for (; *src && o + 3 < dstSize; src++) {
    uint8_t c = (uint8_t)*src;
    bool ok = (c >= 0x20 && c < 0x7F) || (keepUtf8 && c >= 0x80);
    if (!ok) continue;
    if (c == '"' || c == '\\') dst[o++] = '\\';
    dst[o++] = (char)c;
  }
  dst[o] = 0;
}
