#include "nav_heading.h"
#include <cmath>

static const float DEADBAND_DEG = 2.0f;     // ignore sample noise below this
static const float TURN_JUMP_DEG = 40.0f;   // larger changes are real turns, follow faster
static const float ALPHA_NORMAL = 0.5f;
static const float ALPHA_TURN = 0.9f;
static const float TAU_S = 0.22f;           // easing time constant for the displayed angle
static const float MAX_RATE_DPS = 150.0f;   // rotation speed cap
static const float REDRAW_DEG = 1.0f;       // skip redraws for smaller changes
static const float SETTLE_DEG = 0.3f;       // within this of the target, finish the last bit

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
}

void HeadingFilter::onSample(float bearingDeg, bool valid) {
  if (!valid || !std::isfinite(bearingDeg)) {
    return;
  }
  const float bearing = wrap360(bearingDeg);
  if (!known_) {
    target_ = display_ = bearing;
    known_ = true;
    return;
  }
  const float delta = wrap180(bearing - target_);
  if (fabsf(delta) < DEADBAND_DEG) {
    return;
  }
  target_ = wrap360(target_ + delta * (fabsf(delta) > TURN_JUMP_DEG ? ALPHA_TURN : ALPHA_NORMAL));
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

  const float err = wrap180(target_ - display_);
  if (fabsf(err) < 0.05f) {
    display_ = target_;
  } else {
    float step = err * (1.0f - expf(-dt / TAU_S));
    const float maxStep = MAX_RATE_DPS * dt;
    if (step > maxStep) step = maxStep;
    if (step < -maxStep) step = -maxStep;
    display_ = wrap360(display_ + step);
  }

  if (!drawnValid_) {
    return true;
  }
  const float diff = fabsf(wrap180(display_ - drawn_));
  const bool settled = fabsf(wrap180(target_ - display_)) < SETTLE_DEG;
  return diff >= REDRAW_DEG || (settled && diff >= 0.25f);
}

void HeadingFilter::markDrawn() {
  drawn_ = display_;
  drawnValid_ = true;
}
