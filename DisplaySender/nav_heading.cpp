#include "nav_heading.h"
#include <cmath>

/* Tuned on the host against a 90 degree turn with GPS and gyro-fused samples (test/smoothness_report.cpp). */
#ifndef HF_ALPHA
#define HF_ALPHA 0.9f
#endif
#ifndef HF_BETA
#define HF_BETA 0.7f
#endif
#ifndef HF_TAU
#define HF_TAU 0.6f
#endif

static const float DEADBAND_DEG = 2.0f;     // residuals below this are sample noise
static const float TURN_JUMP_DEG = 40.0f;   // larger changes are real turns, follow immediately
static const float ALPHA = HF_ALPHA;        // share of each residual taken into the angle
static const float ALPHA_JUMP = 0.95f;
static const float STEADY_ALPHA = 0.15f;
static const float BETA = HF_BETA;          // share of each residual taken into the turn rate
static const float TAU_CORRECTION_S = HF_TAU; // corrections are absorbed over about this long
static const float RATE_MIN_DPS = 6.0f;     // slower turn rates are noise, not extrapolated
static const float RATE_MAX_DPS = 140.0f;
static const float RATE_HOLD_S = 1.2f;      // keep turning this long without a new sample
static const float RATE_DECAY_S = 0.4f;
static const float MAX_STEP_RATE_DPS = 220.0f;  // rotation speed cap
static const float REDRAW_DEG = 0.15f;      // skip redraws for smaller changes (about half a pixel at the map edge)
static const float SAMPLE_MIN_S = 0.04f;
static const float SAMPLE_MAX_S = 2.0f;

float HeadingFilter::wrap360(float deg) {
  deg = fmodf(deg, 360.0f);
  return deg < 0.0f ? deg + 360.0f : deg;
}

float HeadingFilter::wrap180(float deg) {
  return wrap360(deg + 180.0f) - 180.0f;
}

void HeadingFilter::reset() {
  known_ = false;
  drawnValid_ = false;
  hasTick_ = false;
  hasSample_ = false;
  omega_ = 0.0f;
  pending_ = 0.0f;
}

void HeadingFilter::onSample(float bearingDeg, bool valid) {
  onSample(bearingDeg, valid, lastTickMs_);
}

void HeadingFilter::onSample(float bearingDeg, bool valid, uint32_t nowMs) {
  if (!valid || !std::isfinite(bearingDeg)) {
    hasSample_ = false;  // stationary: hold the last heading, no turning
    omega_ = 0.0f;
    pending_ = 0.0f;
    return;
  }
  const float bearing = wrap360(bearingDeg);
  if (!known_) {
    theta_ = display_ = bearing;
    pending_ = 0.0f;
    omega_ = 0.0f;
    known_ = true;
    hasSample_ = true;
    lastSampleMs_ = nowMs;
    return;
  }

  /* The measurement is taken as current: a gyroscope-fused heading is, and for a GPS course the
     turn-rate extrapolation below covers most of its lag. */
  const float residual = wrap180(bearing - (theta_ + pending_));
  const float dt = hasSample_ ? static_cast<float>(nowMs - lastSampleMs_) * 0.001f : 0.0f;
  lastSampleMs_ = nowMs;
  const bool firstAfterGap = !hasSample_;
  hasSample_ = true;

  if (fabsf(residual) < DEADBAND_DEG && fabsf(omega_) < RATE_MIN_DPS) {
    return;  // steady course with noise
  }
  /* Near-steady course: small residuals are mostly noise, so take them gently. */
  const bool steady = fabsf(omega_) < RATE_MIN_DPS && fabsf(residual) < DEADBAND_DEG * 2.0f;
  pending_ += residual * (fabsf(residual) > TURN_JUMP_DEG ? ALPHA_JUMP : (steady ? STEADY_ALPHA : ALPHA));
  if (!firstAfterGap && dt >= SAMPLE_MIN_S && dt <= SAMPLE_MAX_S) {
    omega_ += BETA * residual / dt;
    if (omega_ > RATE_MAX_DPS) omega_ = RATE_MAX_DPS;
    if (omega_ < -RATE_MAX_DPS) omega_ = -RATE_MAX_DPS;
  }
  if (fabsf(omega_) < RATE_MIN_DPS && fabsf(residual) < DEADBAND_DEG * 2.0f) {
    omega_ = 0.0f;
  }
}

bool HeadingFilter::tick(uint32_t nowMs) {
  if (!known_) {
    return false;
  }
  float dt = hasTick_ ? static_cast<float>(nowMs - lastTickMs_) * 0.001f : 0.0f;
  hasTick_ = true;
  lastTickMs_ = nowMs;
  if (dt > 0.25f) {
    dt = 0.25f;
  }

  float rateStep = 0.0f;
  if (hasSample_ && static_cast<float>(nowMs - lastSampleMs_) * 0.001f > RATE_HOLD_S) {
    omega_ *= expf(-dt / RATE_DECAY_S);  // the samples stopped: wind the turn down
  }
  if (fabsf(omega_) >= RATE_MIN_DPS) {
    rateStep = omega_ * dt;
  }
  float absorbed = pending_ * (1.0f - expf(-dt / TAU_CORRECTION_S));
  float step = rateStep + absorbed;
  const float maxStep = MAX_STEP_RATE_DPS * dt;
  if (step > maxStep) step = maxStep;
  if (step < -maxStep) step = -maxStep;
  absorbed = step - rateStep;  // only what was actually applied leaves the pending correction
  pending_ -= absorbed;
  theta_ = wrap360(theta_ + step);
  display_ = theta_;

  if (!drawnValid_) {
    return true;
  }
  return fabsf(wrap180(display_ - drawn_)) >= REDRAW_DEG;
}

void HeadingFilter::markDrawn() {
  drawn_ = display_;
  drawnValid_ = true;
}
