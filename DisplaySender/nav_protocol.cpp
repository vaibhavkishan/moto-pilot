#include "nav_protocol.h"
#include <ArduinoJson.h>
#include <string.h>

static void copyTruncated(char* dest, size_t destSize, const char* src) {
  if (dest == nullptr || destSize == 0) {
    return;
  }
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, destSize - 1);
  dest[destSize - 1] = '\0';
  size_t valid = 0;
  for (size_t i = 0; dest[i];) {
    const uint8_t lead = static_cast<uint8_t>(dest[i]);
    const size_t bytes = lead < 128 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
    bool complete = true;
    for (size_t n = 1; n < bytes; ++n) {
      if (i + n >= destSize || dest[i + n] == '\0' || (static_cast<uint8_t>(dest[i + n]) & 0xC0) != 0x80) {
        complete = false;
        break;
      }
    }
    if (!complete) {
      break;
    }
    i += bytes;
    valid = i;
  }
  dest[valid] = '\0';
}

bool isLoadingJson(const char* json, size_t length) {
  if (json == nullptr || length < 10) {
    return false;
  }
  return strstr(json, "\"type\":\"loading\"") != nullptr ||
         strstr(json, "\"type\": \"loading\"") != nullptr;
}

bool isArrivedJson(const char* json, size_t length) {
  if (json == nullptr || length < 10) {
    return false;
  }
  return strstr(json, "\"type\":\"arrived\"") != nullptr ||
         strstr(json, "\"type\": \"arrived\"") != nullptr;
}

bool isMotionJson(const char* json, size_t length) {
  if (json == nullptr || length < 10) {
    return false;
  }
  return strstr(json, "\"type\":\"motion\"") != nullptr ||
         strstr(json, "\"type\": \"motion\"") != nullptr;
}

bool parseMotionJson(const char* json, size_t length, MotionUpdate& update) {
  JsonDocument doc;
  if (deserializeJson(doc, json, length)) {
    return false;
  }
  if (!doc["lat"].is<double>() || !doc["lon"].is<double>()) {
    return false;
  }
  update.lat = doc["lat"].as<double>();
  update.lon = doc["lon"].as<double>();
  update.bearing = doc["bearing"] | 0.0f;
  update.speedMps = doc["speed_mps"] | 0.0f;
  update.bearingValid = doc["bearing_valid"] | false;
  update.gpsWeak = doc["gps_weak"] | false;
  update.ageMs = static_cast<uint16_t>(doc["age_ms"] | 0);
  return true;
}

bool parseNavJson(const char* json, size_t length, NavState& state) {
  if (json == nullptr || length == 0) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json, length);
  if (error) {
    Serial.printf("JSON error: %s\n", error.c_str());
    return false;
  }

  const char* type = doc["type"] | "";
  if (strcmp(type, "heartbeat") == 0) {
    return false;
  }
  if (strcmp(type, "loading") == 0) {
    return false;
  }
  if (strcmp(type, "nav") != 0) {
    return false;
  }

  state.valid = true;
  state.lat = doc["lat"] | 0.0;
  state.lon = doc["lon"] | 0.0;
  state.bearing = doc["bearing"] | 0.0f;
  state.speedMps = doc["speed_mps"] | 0.0f;
  state.pxPerM = doc["px_per_m"] | 0.0f;
  state.style = strcmp(doc["style"] | "", "minimal") == 0 ? STYLE_MINIMAL : STYLE_CLASSIC;
  state.tripProgress = doc["progress"] | -1.0f;
  state.zoomMax = doc["zoom_max"] | 3.5f;
  state.zoomLeadS = doc["zoom_lead_s"] | 8.0f;
  state.ageMs = static_cast<uint16_t>(doc["age_ms"] | 0);
  state.headingUpMode = doc["bearing_valid"].is<bool>();
  state.bearingValid = doc["bearing_valid"] | false;
  state.distanceM = doc["distance_m"] | 0;
  state.remainingM = doc["remaining_m"] | -1;
  state.remainingS = doc["remaining_s"] | -1;
  state.offRoute = doc["off_route"] | false;
  state.gpsWeak = doc["gps_weak"] | false;
  state.rerouting = doc["rerouting"] | false;
  state.maneuver = maneuverFromName(doc["maneuver"] | "");
  copyTruncated(state.eta, sizeof(state.eta), doc["eta"] | "");
  copyTruncated(state.instruction, sizeof(state.instruction), doc["instruction"] | "");
  copyTruncated(state.street, sizeof(state.street), doc["street"] | "");

  const char* htmlField = doc["html"] | "";
  if (htmlField[0] != '\0') {
    copyTruncated(state.html, sizeof(state.html), htmlField);
    state.hasHtml = true;
  } else {
    state.html[0] = '\0';
    state.hasHtml = false;
  }

  state.routeCount = 0;
  state.routeDoneCount = 0;
  state.hasDest = false;
  state.streetSegmentCount = 0;
  state.hasUserPosition = false;

  JsonArray route = doc["route"].as<JsonArray>();
  if (!route.isNull()) {
    for (JsonArray point : route) {
      if (state.routeCount >= NAV_MAX_ROUTE_POINTS) {
        break;
      }
      if (point.size() < 2) {
        continue;
      }
      state.routeX[state.routeCount] = point[0].as<int16_t>();
      state.routeY[state.routeCount] = point[1].as<int16_t>();
      state.routeCount++;
    }
  }

  JsonArray done = doc["route_done"].as<JsonArray>();
  if (!done.isNull()) {
    for (JsonArray point : done) {
      if (state.routeDoneCount >= NAV_MAX_DONE_POINTS) {
        break;
      }
      if (point.size() < 2) {
        continue;
      }
      state.routeDoneX[state.routeDoneCount] = point[0].as<int16_t>();
      state.routeDoneY[state.routeDoneCount] = point[1].as<int16_t>();
      state.routeDoneCount++;
    }
  }

  if (doc["dest_x"].is<int>() && doc["dest_y"].is<int>()) {
    state.destX = doc["dest_x"].as<int16_t>();
    state.destY = doc["dest_y"].as<int16_t>();
    state.hasDest = true;
  }

  JsonArray streets = doc["streets"].as<JsonArray>();
  if (!streets.isNull()) {
    for (JsonArray segment : streets) {
      if (state.streetSegmentCount >= NAV_MAX_STREET_SEGMENTS) {
        break;
      }
      if (segment.size() < 4) {
        continue;
      }
      state.streetX0[state.streetSegmentCount] = segment[0].as<int16_t>();
      state.streetY0[state.streetSegmentCount] = segment[1].as<int16_t>();
      state.streetX1[state.streetSegmentCount] = segment[2].as<int16_t>();
      state.streetY1[state.streetSegmentCount] = segment[3].as<int16_t>();
      state.streetSegmentCount++;
    }
  }

  if (doc["user_x"].is<int>() && doc["user_y"].is<int>()) {
    state.userX = doc["user_x"].as<int16_t>();
    state.userY = doc["user_y"].as<int16_t>();
    state.hasUserPosition = true;
  } else if (state.routeCount > 0) {
    state.userX = state.routeX[state.routeCount - 1];
    state.userY = state.routeY[state.routeCount - 1];
    state.hasUserPosition = state.userX >= 0 && state.userY >= 0;
  }

  return true;
}
