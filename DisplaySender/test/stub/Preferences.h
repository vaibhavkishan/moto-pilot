#pragma once
// Preferences stand-in: nothing is stored, defaults are returned.
#include <stdint.h>

class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  uint8_t getUChar(const char*, uint8_t fallback = 0) { return fallback; }
  void putUChar(const char*, uint8_t) {}
  void end() {}
};
