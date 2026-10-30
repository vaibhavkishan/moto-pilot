#pragma once
// Minimal Arduino core stand-ins for host previews.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define PROGMEM
#define memcpy_P memcpy
#define F(x) (x)

struct SerialStub {
  template <class... A>
  void printf(const char*, A...) {}
  template <class... A>
  void println(A...) {}
  template <class... A>
  void print(A...) {}
};
inline SerialStub Serial;

struct EspStub {
  uint32_t getFreeHeap() { return 100000; }
  uint32_t getMinFreeHeap() { return 100000; }
  uint32_t getMaxAllocHeap() { return 100000; }
};
inline EspStub ESP;

inline uint32_t micros() { return 0; }
inline uint32_t millis() { return 0; }
