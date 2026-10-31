#pragma once

#include <stdint.h>

/**
 * Zooms the classic map in as a turn gets close and back out after it, like Google Maps.
 * Hardware independent. The zoom scales the canvas around the user; it never goes below 1,
 * because the phone only sends a fixed window of geometry.
 */
class ZoomController {
public:
  void reset();

  /**
   * Set where the zoom should head. `distanceM` is the distance to the next maneuver, already
   * advanced to the current time. With active=false (off route, weak GPS, other style) it eases back to 1.
   * `maxZoom` is the most it may zoom (1 or less = never), `leadS` how many seconds before the turn it starts.
   */
  void setTarget(bool active, uint8_t maneuver, float distanceM, float speedMps, float pxPerM, float maxZoom,
                 float leadS);

  /** Move toward the target. Returns true while the zoom is still changing. */
  bool tick(uint32_t nowMs);

  float zoom() const { return zoom_; }

  /** Zoom wanted for a maneuver at this distance (1 = no zoom). */
  static float targetFor(uint8_t maneuver, float distanceM, float speedMps, float pxPerM, float maxZoom,
                         float leadS);

  /** Maneuvers worth zooming for: real turns, roundabouts and the arrival. */
  static bool isTurn(uint8_t maneuver);

private:
  float zoom_ = 1.0f;
  float target_ = 1.0f;
  uint32_t lastTickMs_ = 0;
  bool hasTick_ = false;
};
