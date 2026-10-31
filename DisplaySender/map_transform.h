#pragma once

#include <math.h>
#include <stdint.h>
#include "nav_types.h"

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251f
#endif

/** Presentation state derived from the heading filter, not from the BLE frame. */
struct MapView {
  bool headingKnown = false;
  float headingDeg = 0.0f;  // smoothed direction of travel, clockwise from north
  /* Predicted movement of the user since the frame, in canvas pixels (x east, y south). */
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  /* Scale around the user: 1 is the phone's scale, larger zooms in. */
  float zoom = 1.0f;
};

/*
 * Source geometry is in the phone's canvas pixels, centred on the user at (ux, uy).
 * Heading-up: translate the user to the screen anchor and rotate so the travel
 * direction points up. Frames without bearing_valid: draw as received.
 */
struct MapXf {
  bool rotate = false;
  float c = 1.0f;
  float s = 0.0f;
  float ux = 0.0f, uy = 0.0f;
  float zoom = 1.0f;
  int ax = 0, ay = 0;
};

inline MapXf makeXf(const NavState& state, const MapView& view) {
  MapXf xf;
  xf.zoom = view.zoom > 0.1f ? view.zoom : 1.0f;
  if (state.hasUserPosition) {
    xf.ux = state.userX;
    xf.uy = state.userY;
  }
  if (state.headingUpMode) {
    xf.ux += view.offsetX;
    xf.uy += view.offsetY;
    xf.ax = MAP_ANCHOR_X;
    xf.ay = MAP_ANCHOR_Y;
    if (view.headingKnown) {
      const float rad = view.headingDeg * DEG_TO_RAD;
      xf.c = cosf(rad);
      xf.s = sinf(rad);
      xf.rotate = true;
    }
  } else {
    xf.ax = static_cast<int>(lroundf(xf.ux));
    xf.ay = static_cast<int>(lroundf(xf.uy));
  }
  return xf;
}

inline void xfApply(const MapXf& xf, int16_t x, int16_t y, int16_t& ox, int16_t& oy) {
  const float dx = static_cast<float>(x - xf.ux) * xf.zoom;
  const float dy = static_cast<float>(y - xf.uy) * xf.zoom;
  if (xf.rotate) {
    ox = static_cast<int16_t>(xf.ax + lroundf(dx * xf.c + dy * xf.s));
    oy = static_cast<int16_t>(xf.ay + lroundf(dy * xf.c - dx * xf.s));
  } else {
    ox = static_cast<int16_t>(xf.ax + lroundf(dx));
    oy = static_cast<int16_t>(xf.ay + lroundf(dy));
  }
}
