#include "motion_predictor.h"

#include <cmath>

static const float DEG_TO_RAD_F = 0.01745329251f;
static const double METERS_PER_DEG_LAT = 111320.0;
static const float CORRECTION_TAU_S = 0.35f;   // how fast a GPS correction is absorbed
static const float MAX_JUMP_PX = 60.0f;        // a bigger gap is a teleport (reroute, new trip): snap
static const uint32_t MAX_PREDICT_MS = 2000;   // stop advancing when fixes stop arriving
static const float MIN_SPEED_MPS = 0.5f;
static const float SCALE_TOLERANCE = 0.01f;

void MotionPredictor::reset() {
  hasFrame_ = false;
  moving_ = false;
  speed_ = 0.0f;
  fixX_ = fixY_ = offX_ = offY_ = advX_ = advY_ = 0.0f;
}

float MotionPredictor::advancedMeters() const {
  if (!(pxPerM_ > 0.0f)) {
    return 0.0f;
  }
  return std::hypot(fixX_ + advX_, fixY_ + advY_) / pxPerM_;
}

void MotionPredictor::onFrame(double lat, double lon, float speedMps, float bearingDeg, bool moving,
                              float pxPerM, uint32_t ageMs, uint32_t nowMs) {
  /* A different map scale cannot be compared with the old drawn position: start over. */
  const bool sameScale = hasFrame_ && pxPerM_ > 0.0f && pxPerM > 0.0f &&
                         std::fabs(pxPerM / pxPerM_ - 1.0f) < SCALE_TOLERANCE;
  if (!sameScale) {
    hasFrame_ = false;
  }
  rebase(lat, lon, speedMps, bearingDeg, moving, pxPerM > 0.0f ? pxPerM : pxPerM_, ageMs, nowMs, true);
}

void MotionPredictor::onFix(double lat, double lon, float speedMps, float bearingDeg, bool moving,
                            uint32_t ageMs, uint32_t nowMs) {
  if (!hasFrame_) {
    return;  // no canvas yet to place the fix on
  }
  rebase(lat, lon, speedMps, bearingDeg, moving, pxPerM_, ageMs, nowMs, false);
}

void MotionPredictor::rebase(double lat, double lon, float speedMps, float bearingDeg, bool moving,
                             float pxPerM, uint32_t ageMs, uint32_t nowMs, bool newCanvas) {
  const bool hadDrawn = hasFrame_;
  /* Where the map is drawn now, relative to the old canvas centre. */
  float drawnX = fixX_ + offX_ + advX_;
  float drawnY = fixY_ + offY_ + advY_;

  if (newCanvas) {
    if (hadDrawn && pxPerM > 0.0f) {
      /* Express the drawn position against the new centre. */
      const double metersPerDegLon = METERS_PER_DEG_LAT * std::cos(lat * DEG_TO_RAD_F);
      drawnX += static_cast<float>((centerLon_ - lon) * metersPerDegLon) * pxPerM;
      drawnY += -static_cast<float>((centerLat_ - lat) * METERS_PER_DEG_LAT) * pxPerM;
    }
    centerLat_ = lat;
    centerLon_ = lon;
    pxPerM_ = pxPerM;
    fixX_ = fixY_ = 0.0f;
  } else if (pxPerM_ > 0.0f) {
    const double metersPerDegLon = METERS_PER_DEG_LAT * std::cos(centerLat_ * DEG_TO_RAD_F);
    fixX_ = static_cast<float>((lon - centerLon_) * metersPerDegLon) * pxPerM_;
    fixY_ = -static_cast<float>((lat - centerLat_) * METERS_PER_DEG_LAT) * pxPerM_;
  }

  speed_ = speedMps;
  moving_ = moving && pxPerM_ > 0.0f && std::isfinite(speedMps) && speedMps >= MIN_SPEED_MPS;
  advX_ = advY_ = 0.0f;
  if (moving_ && std::isfinite(bearingDeg)) {
    const float d = speed_ * (static_cast<float>(ageMs) * 0.001f) * pxPerM_;
    advX_ = d * std::sin(bearingDeg * DEG_TO_RAD_F);
    advY_ = -d * std::cos(bearingDeg * DEG_TO_RAD_F);
  }

  /* Keep the drawn position where it was; the correction then fades out. */
  offX_ = hadDrawn ? drawnX - fixX_ - advX_ : 0.0f;
  offY_ = hadDrawn ? drawnY - fixY_ - advY_ : 0.0f;
  if (std::hypot(offX_, offY_) > MAX_JUMP_PX) {
    offX_ = offY_ = 0.0f;
  }
  hasFrame_ = true;
  fixMs_ = nowMs;
  lastTickMs_ = nowMs;
}

bool MotionPredictor::tick(uint32_t nowMs, float headingDeg) {
  if (!hasFrame_) {
    return false;
  }
  float dt = static_cast<float>(nowMs - lastTickMs_) * 0.001f;
  lastTickMs_ = nowMs;
  if (dt < 0.0f) dt = 0.0f;
  if (dt > 0.25f) dt = 0.25f;

  bool changed = false;
  if (moving_ && nowMs - fixMs_ < MAX_PREDICT_MS && dt > 0.0f) {
    const float d = speed_ * dt * pxPerM_;
    advX_ += d * std::sin(headingDeg * DEG_TO_RAD_F);
    advY_ -= d * std::cos(headingDeg * DEG_TO_RAD_F);
    changed = true;
  }
  if (std::fabs(offX_) + std::fabs(offY_) > 0.02f) {
    const float k = std::exp(-dt / CORRECTION_TAU_S);
    offX_ *= k;
    offY_ *= k;
    changed = true;
  } else {
    offX_ = offY_ = 0.0f;
  }
  return changed;
}
