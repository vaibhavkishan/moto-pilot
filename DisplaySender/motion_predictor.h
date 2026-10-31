#pragma once

#include <stdint.h>

/**
 * Dead-reckons the user between GPS frames so the map glides instead of stepping. Hardware independent.
 *
 * Offsets are canvas pixels relative to the centre of the latest full frame (x east, y south).
 * The drawn position is: the latest fix (relative to that centre) + a correction that fades out +
 * the distance covered since the fix. A new fix, from a full frame or from a light motion update,
 * keeps the drawn position continuous and lets the correction fade, so the picture never jumps.
 */
class MotionPredictor {
public:
  void reset();

  /**
   * A full frame arrived at nowMs and re-centres the canvas on its fix. `moving` must be false for a
   * stationary or weak-GPS fix. `ageMs` is how old the fix already was when the phone sent it.
   */
  void onFrame(double lat, double lon, float speedMps, float bearingDeg, bool moving, float pxPerM,
               uint32_t ageMs, uint32_t nowMs);

  /** A light motion update: a newer fix or heading, without a new canvas. */
  void onFix(double lat, double lon, float speedMps, float bearingDeg, bool moving, uint32_t ageMs,
             uint32_t nowMs);

  /** Advance to nowMs, travelling along headingDeg. Returns true while the offset is still changing. */
  bool tick(uint32_t nowMs, float headingDeg);

  float offsetX() const { return fixX_ + offX_ + advX_; }
  float offsetY() const { return fixY_ + offY_ + advY_; }

  /** Metres the user has moved from the centre of the last full frame, ignoring the fading correction. */
  float advancedMeters() const;

private:
  void rebase(double lat, double lon, float speedMps, float bearingDeg, bool moving, float pxPerM,
              uint32_t ageMs, uint32_t nowMs, bool newCanvas);

  bool hasFrame_ = false;
  bool moving_ = false;
  double centerLat_ = 0.0;  // centre of the canvas, from the last full frame
  double centerLon_ = 0.0;
  float speed_ = 0.0f;
  float pxPerM_ = 0.0f;
  uint32_t fixMs_ = 0;
  uint32_t lastTickMs_ = 0;
  float fixX_ = 0.0f;  // the latest fix, relative to the canvas centre
  float fixY_ = 0.0f;
  float offX_ = 0.0f;  // fading correction between the drawn position and the fix
  float offY_ = 0.0f;
  float advX_ = 0.0f;  // distance covered since the fix, from speed and heading
  float advY_ = 0.0f;
};
