#pragma once

#include <stdint.h>

/**
 * Smooths the direction of travel for map rotation. Hardware independent.
 *
 * A rate-tracking filter: it follows the heading samples and also learns how fast the heading is
 * turning, so the shown angle keeps moving between samples instead of stalling and jumping.
 * Samples flagged invalid (stationary, weak GPS, no course) are ignored, so the last good heading
 * is held instead of drifting or snapping to north.
 */
class HeadingFilter {
public:
  void reset();

  /** Feed a heading from a frame. Invalid or non-finite samples are ignored. */
  void onSample(float bearingDeg, bool valid);

  /** Same, with timing so the turn rate can be learned from the samples. */
  void onSample(float bearingDeg, bool valid, uint32_t nowMs);

  /** Advance the displayed angle. Returns true when it moved enough to redraw. */
  bool tick(uint32_t nowMs);

  /** Call after rendering with the angle returned by display(). */
  void markDrawn();

  bool known() const { return known_; }
  float display() const { return display_; }
  /** Turn rate in degrees per second, clockwise positive; 0 when steady. */
  float rate() const { return omega_; }

  static float wrap360(float deg);
  /** Shortest signed difference, in [-180, 180). 359 -> 1 gives +2. */
  static float wrap180(float deg);

private:
  float theta_ = 0.0f;    // tracked heading of the measured stream
  float pending_ = 0.0f;  // correction still to be absorbed
  float omega_ = 0.0f;
  float display_ = 0.0f;
  float drawn_ = 0.0f;
  bool known_ = false;
  bool drawnValid_ = false;
  bool hasTick_ = false;
  bool hasSample_ = false;
  uint32_t lastTickMs_ = 0;
  uint32_t lastSampleMs_ = 0;
};
