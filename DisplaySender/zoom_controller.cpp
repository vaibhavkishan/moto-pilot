#include "zoom_controller.h"

#include <cmath>

#include "nav_types.h"

static const float MAX_SCALE_PX_PER_M = 2.5f;  // never zoom beyond about 90 m across the map height
static const float MIN_USEFUL_ZOOM = 1.05f;
static const float NEAR_M = 40.0f;         // at this distance the zoom is at its maximum
static const float FAR_MIN_M = 80.0f;      // zooming starts at least this far from the turn
static const float FAR_MAX_M = 450.0f;
/* A roundabout stays zoomed in from entry to exit, whose step ends up to ~150 m after the entry. */
static const float ROUNDABOUT_NEAR_M = 150.0f;
static const float ROUNDABOUT_FAR_M = 250.0f;
static const float TAU_IN_S = 0.5f;
static const float TAU_OUT_S = 0.7f;       // easing back out is gentler
static const float MAX_RATE_PER_S = 1.5f;

void ZoomController::reset() {
  zoom_ = 1.0f;
  target_ = 1.0f;
  hasTick_ = false;
}

bool ZoomController::isTurn(uint8_t maneuver) {
  switch (maneuver) {
    case MAN_LEFT:
    case MAN_RIGHT:
    case MAN_SLIGHT_LEFT:
    case MAN_SLIGHT_RIGHT:
    case MAN_SHARP_LEFT:
    case MAN_SHARP_RIGHT:
    case MAN_UTURN:
    case MAN_ROUNDABOUT:
    case MAN_ARRIVE:
      return true;
    default:
      return false;
  }
}

float ZoomController::targetFor(uint8_t maneuver, float distanceM, float speedMps, float pxPerM, float maxZoom,
                                float leadS) {
  if (!isTurn(maneuver) || !(pxPerM > 0.0f) || !std::isfinite(distanceM) || !(maxZoom > 1.0f)) {
    return 1.0f;
  }
  float limit = MAX_SCALE_PX_PER_M / pxPerM;
  if (limit > maxZoom) limit = maxZoom;
  if (limit < MIN_USEFUL_ZOOM) {
    return 1.0f;  // the map is already close enough
  }
  float farM = std::isfinite(speedMps) ? speedMps * leadS : FAR_MIN_M;
  if (farM < FAR_MIN_M) farM = FAR_MIN_M;
  if (farM > FAR_MAX_M) farM = FAR_MAX_M;
  float nearM = NEAR_M;
  if (maneuver == MAN_ROUNDABOUT) {
    nearM = ROUNDABOUT_NEAR_M;
    if (farM < ROUNDABOUT_FAR_M) farM = ROUNDABOUT_FAR_M;
  }
  float t = (farM - distanceM) / (farM - nearM);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  const float eased = t * t * (3.0f - 2.0f * t);
  return 1.0f + (limit - 1.0f) * eased;
}

void ZoomController::setTarget(bool active, uint8_t maneuver, float distanceM, float speedMps, float pxPerM,
                               float maxZoom, float leadS) {
  target_ = active ? targetFor(maneuver, distanceM, speedMps, pxPerM, maxZoom, leadS) : 1.0f;
}

bool ZoomController::tick(uint32_t nowMs) {
  float dt = hasTick_ ? static_cast<float>(nowMs - lastTickMs_) * 0.001f : 0.0f;
  hasTick_ = true;
  lastTickMs_ = nowMs;
  if (dt > 0.25f) dt = 0.25f;

  const float err = target_ - zoom_;
  if (std::fabs(err) < 0.002f) {
    const bool changed = zoom_ != target_;
    zoom_ = target_;
    return changed;
  }
  float step = err * (1.0f - std::exp(-dt / (err > 0.0f ? TAU_IN_S : TAU_OUT_S)));
  const float maxStep = MAX_RATE_PER_S * dt;
  if (step > maxStep) step = maxStep;
  if (step < -maxStep) step = -maxStep;
  zoom_ += step;
  return std::fabs(step) > 1e-5f;
}
