#pragma once

#include <stdint.h>
#include <string.h>

#define NAV_MAX_ROUTE_POINTS 64
#define NAV_MAX_DONE_POINTS 32
#define NAV_MAX_STREET_SEGMENTS 192
#define SCR_W 240
#define SCR_H 320
#define MAP_AREA_H 232
#define OVERLAY_H (SCR_H - MAP_AREA_H)
#define NAV_HTML_MAX 480
/* Where the user sits on screen when the map is rotated heading-up (lower third, like Google Maps). */
#define MAP_ANCHOR_X (SCR_W / 2)
#define MAP_ANCHOR_Y 160

/* Icon codes for the next maneuver; names match the `maneuver` field sent by the app. */
enum NavManeuver : uint8_t {
  MAN_NONE = 0,
  MAN_DEPART,
  MAN_ARRIVE,
  MAN_STRAIGHT,
  MAN_LEFT,
  MAN_RIGHT,
  MAN_SLIGHT_LEFT,
  MAN_SLIGHT_RIGHT,
  MAN_SHARP_LEFT,
  MAN_SHARP_RIGHT,
  MAN_UTURN,
  MAN_ROUNDABOUT
};

inline NavManeuver maneuverFromName(const char* name) {
  struct Entry { const char* name; NavManeuver value; };
  static const Entry table[] = {
    {"depart", MAN_DEPART},           {"arrive", MAN_ARRIVE},
    {"straight", MAN_STRAIGHT},       {"left", MAN_LEFT},
    {"right", MAN_RIGHT},             {"slight_left", MAN_SLIGHT_LEFT},
    {"slight_right", MAN_SLIGHT_RIGHT}, {"sharp_left", MAN_SHARP_LEFT},
    {"sharp_right", MAN_SHARP_RIGHT}, {"uturn", MAN_UTURN},
    {"roundabout", MAN_ROUNDABOUT},
  };
  if (name == nullptr) return MAN_NONE;
  for (const Entry& entry : table) {
    if (strcmp(name, entry.name) == 0) return entry.value;
  }
  return MAN_NONE;
}

struct NavState {
  bool valid = false;
  double lat = 0.0;
  double lon = 0.0;
  float bearing = 0.0f;
  float speedMps = 0.0f;
  /* True only when the app sent bearing_valid (heading-up protocol); false when the field is absent. */
  bool headingUpMode = false;
  bool bearingValid = false;
  int distanceM = 0;
  int remainingM = -1;
  int remainingS = -1;
  bool offRoute = false;
  bool rerouting = false;
  bool gpsWeak = false;
  uint8_t maneuver = MAN_NONE;
  char eta[8] = {};  // arrival clock time, "HH:MM"; empty when unknown
  char instruction[128] = {};
  char street[96] = {};
  char html[NAV_HTML_MAX] = {};
  bool hasHtml = false;
  int routeCount = 0;
  int16_t routeX[NAV_MAX_ROUTE_POINTS] = {};
  int16_t routeY[NAV_MAX_ROUTE_POINTS] = {};
  /* Part of the route already travelled, drawn grey. */
  int routeDoneCount = 0;
  int16_t routeDoneX[NAV_MAX_DONE_POINTS] = {};
  int16_t routeDoneY[NAV_MAX_DONE_POINTS] = {};
  int16_t destX = 0;
  int16_t destY = 0;
  bool hasDest = false;
  int streetSegmentCount = 0;
  int16_t streetX0[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t streetY0[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t streetX1[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t streetY1[NAV_MAX_STREET_SEGMENTS] = {};
  int16_t userX = 0;
  int16_t userY = 0;
  bool hasUserPosition = false;
};
