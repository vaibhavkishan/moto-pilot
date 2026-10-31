// Host measurement of the CYD smoothing pipeline against a known ride. Not part of the firmware.
//   python -m ziglang c++ -std=c++17 -I DisplaySender DisplaySender/test/smoothness_report.cpp DisplaySender/nav_heading.cpp DisplaySender/motion_predictor.cpp -o smooth.exe
#include <cmath>
#include <cstdio>
#include <vector>

#include "motion_predictor.h"
#include "nav_heading.h"

namespace {

const float kDeg = 0.01745329251f;

// A ride at 12 m/s that turns 90 degrees to the right between t=4 s and t=7 s.
float truthHeading(float t) {
  if (t < 4.0f) return 0.0f;
  if (t > 7.0f) return 90.0f;
  const float u = (t - 4.0f) / 3.0f;
  return 90.0f * (u * u * (3.0f - 2.0f * u));
}

struct Stats {
  double headingRms = 0, headingMax = 0, stepMax = 0, jerkMax = 0, lagSec = 0;
};

Stats run(float frameHz, uint16_t lagMs, float noiseDeg) {
  HeadingFilter filter;
  const float frameDt = 1.0f / frameHz;
  float nextFrame = 0.0f;
  float previous = -1000.0f, previousStep = 0.0f;
  Stats s;
  int n = 0;
  unsigned rng = 12345;
  auto noise = [&]() {
    rng = rng * 1664525u + 1013904223u;
    return (static_cast<int>((rng >> 8) % 2001) - 1000) / 1000.0f * noiseDeg;
  };
  for (int ms = 0; ms <= 10000; ms += 20) {
    const float t = ms * 0.001f;
    if (t >= nextFrame) {
      nextFrame += frameDt;
      // GPS course lags the real heading by about 0.8 s; a fused heading by almost nothing.
      const float course = truthHeading(t - lagMs * 0.001f) + noise();
      filter.onSample(course, true, static_cast<uint32_t>(ms));
    }
    filter.tick(static_cast<uint32_t>(ms));
    if (!filter.known()) continue;
    const float shown = filter.display();
    const float err = HeadingFilter::wrap180(shown - truthHeading(t));
    s.headingRms += err * err;
    if (std::fabs(err) > s.headingMax) s.headingMax = std::fabs(err);
    if (previous > -999.0f) {
      const float step = HeadingFilter::wrap180(shown - previous);
      if (std::fabs(step) > s.stepMax) s.stepMax = std::fabs(step);
      const float jerk = std::fabs(step - previousStep);
      if (t > 3.0f && t < 9.0f && jerk > s.jerkMax) s.jerkMax = jerk;
      previousStep = step;
    }
    previous = shown;
    n++;
  }
  s.headingRms = std::sqrt(s.headingRms / n);
  // Time the shown heading takes to reach 45 degrees, against the true time.
  HeadingFilter f2;
  nextFrame = 0.0f;
  for (int ms = 0; ms <= 10000; ms += 20) {
    const float t = ms * 0.001f;
    if (t >= nextFrame) {
      nextFrame += frameDt;
      const float course = truthHeading(t - lagMs * 0.001f);
      f2.onSample(course, true, static_cast<uint32_t>(ms));
    }
    f2.tick(static_cast<uint32_t>(ms));
    if (f2.known() && f2.display() >= 45.0f && t > 4.0f) { s.lagSec = t - 5.5f; break; }  // truth is 45 at 5.5 s
  }
  return s;
}

}  // namespace

int main() {
  std::printf("%-34s %8s %8s %9s %9s %7s\n", "case (turn: 90 deg in 3 s)", "rms err", "max err", "max step", "max jerk", "lag s");
  struct Case { const char* name; float hz; uint16_t lag; float noise; } cases[] = {
      {"GPS 1 Hz (course 0.8 s late)", 1, 800, 1.0f}, {"GPS 2 Hz (course 0.8 s late)", 2, 800, 1.0f},
      {"GPS 5 Hz (course 0.8 s late)", 5, 800, 1.0f}, {"GPS 2 Hz, 0.4 s late", 2, 400, 1.0f},
      {"fused (gyro) 10 Hz", 10, 0, 0.3f},            {"fused (gyro) 5 Hz", 5, 0, 0.3f}};
  for (const Case& c : cases) {
    const Stats s = run(c.hz, c.lag, c.noise);
    std::printf("%-34s %8.2f %8.2f %9.2f %9.2f %7.2f\n", c.name, s.headingRms, s.headingMax, s.stepMax, s.jerkMax, s.lagSec);
  }
  return 0;
}
