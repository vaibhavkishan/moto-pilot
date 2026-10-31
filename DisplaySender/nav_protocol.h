#pragma once

#include <stddef.h>
#include "nav_types.h"

/** Parse JSON nav message into NavState. Requires ArduinoJson library. */
bool parseNavJson(const char* json, size_t length, NavState& state);

/** True when message is {"type":"loading"} (show loading UI on CYD). */
bool isLoadingJson(const char* json, size_t length);

/** True when message is {"type":"arrived"} (show the arrival screen). */
bool isArrivedJson(const char* json, size_t length);

/** A light update sent between full frames: the newest fix and heading, no map geometry. */
struct MotionUpdate {
  double lat = 0.0;
  double lon = 0.0;
  float bearing = 0.0f;
  float speedMps = 0.0f;
  bool bearingValid = false;
  bool gpsWeak = false;
  uint16_t ageMs = 0;
};

/** True when message is {"type":"motion", ...}. */
bool isMotionJson(const char* json, size_t length);

bool parseMotionJson(const char* json, size_t length, MotionUpdate& update);
