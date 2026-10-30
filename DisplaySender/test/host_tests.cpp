// Host-side tests for the pure CYD logic (no hardware). Build and run:
//   python -m ziglang c++ -std=c++17 -I DisplaySender DisplaySender/test/host_tests.cpp DisplaySender/nav_heading.cpp -o host_tests.exe
// Any C++17 compiler works. Arduino ignores this folder.
#include <cmath>
#include <cstdio>
#include <memory>

#include "nav_heading.h"
#include "map_transform.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                    \
  do {                                                                 \
    checks++;                                                          \
    if (!(cond)) {                                                     \
      failures++;                                                      \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
    }                                                                  \
  } while (0)

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static void testWrap() {
  CHECK(near(HeadingFilter::wrap180(1.0f - 359.0f), 2.0f, 1e-4f));
  CHECK(near(HeadingFilter::wrap180(359.0f - 1.0f), -2.0f, 1e-4f));
  CHECK(near(HeadingFilter::wrap360(-10.0f), 350.0f, 1e-4f));
  CHECK(near(HeadingFilter::wrap360(720.0f), 0.0f, 1e-4f));
  CHECK(near(HeadingFilter::wrap180(180.0f), -180.0f, 1e-4f));
}

static void testUnknownUntilValidSample() {
  HeadingFilter f;
  CHECK(!f.known());
  f.onSample(90.0f, false);
  f.onSample(NAN, true);
  CHECK(!f.known());
  CHECK(!f.tick(0));
  f.onSample(90.0f, true);
  CHECK(f.known());
  CHECK(near(f.display(), 90.0f, 1e-4f));
}

static void testCrossingNorthTakesTheShortWay() {
  HeadingFilter f;
  f.onSample(358.0f, true);
  f.tick(0);
  f.markDrawn();
  f.onSample(2.0f, true);  // 4 degrees clockwise across north
  float distanceTravelled = 0.0f;
  float previous = f.display();
  for (uint32_t t = 50; t <= 2000; t += 50) {
    f.tick(t);
    const float now = f.display();
    CHECK(now >= 350.0f || now <= 10.0f);  // never sweeps the long way round
    distanceTravelled += std::fabs(HeadingFilter::wrap180(now - previous));
    previous = now;
  }
  CHECK(distanceTravelled < 6.0f);
  CHECK(near(HeadingFilter::wrap180(f.display() - 0.0f), 0.0f, 0.6f));  // target is 358 + 4*0.5 = 0
}

static void testNoiseInsideDeadbandIsIgnored() {
  HeadingFilter f;
  f.onSample(90.0f, true);
  f.tick(0);
  f.markDrawn();
  f.onSample(91.5f, true);
  f.onSample(88.7f, true);
  bool redraw = false;
  for (uint32_t t = 50; t <= 1000; t += 50) redraw |= f.tick(t);
  CHECK(!redraw);
  CHECK(near(f.display(), 90.0f, 1e-4f));
}

static void testSlowDriftEventuallyFollows() {
  HeadingFilter f;
  f.onSample(90.0f, true);
  for (int i = 1; i <= 8; i++) f.onSample(90.0f + i * 0.7f, true);  // 5.6 degrees total
  for (uint32_t t = 0; t <= 3000; t += 50) f.tick(t);
  CHECK(f.display() > 91.0f);
}

static void testStationaryHoldsHeadingAndNeverRedraws() {
  HeadingFilter f;
  f.onSample(200.0f, true);
  f.tick(0);
  f.markDrawn();
  bool redraw = false;
  for (uint32_t t = 50; t <= 60000; t += 50) {
    f.onSample(10.0f + static_cast<float>(t % 7), false);  // garbage flagged invalid
    redraw |= f.tick(t);
  }
  CHECK(!redraw);
  CHECK(near(f.display(), 200.0f, 1e-4f));
}

static void testRotationRateIsCapped() {
  HeadingFilter f;
  f.onSample(0.0f, true);
  f.tick(0);
  f.markDrawn();
  f.onSample(170.0f, true);  // near U-turn (exactly 180 is ambiguous)
  float previous = f.display();
  float maxStep = 0.0f;
  for (uint32_t t = 50; t <= 4000; t += 50) {
    f.tick(t);
    const float step = std::fabs(HeadingFilter::wrap180(f.display() - previous));
    if (step > maxStep) maxStep = step;
    previous = f.display();
  }
  CHECK(maxStep <= 150.0f * 0.05f + 0.01f);
  CHECK(near(HeadingFilter::wrap180(f.display() - 153.0f), 0.0f, 1.0f));  // 0 + 170 * 0.9
}

static void testRedrawOnlyWhenAngleMovedEnough() {
  HeadingFilter f;
  f.onSample(90.0f, true);
  f.tick(0);
  f.markDrawn();
  f.onSample(100.0f, true);  // real turn, target 95
  int redraws = 0;
  for (uint32_t t = 50; t <= 3000; t += 50) {
    if (f.tick(t)) {
      redraws++;
      f.markDrawn();
    }
  }
  CHECK(redraws >= 3 && redraws <= 8);  // a handful of redraws, not one per tick (59 ticks)
  CHECK(near(f.display(), 95.0f, 0.5f));
}

static void testResetForgetsHeading() {
  HeadingFilter f;
  f.onSample(10.0f, true);
  f.reset();
  CHECK(!f.known());
  CHECK(!f.tick(100));
}

static std::unique_ptr<NavState> headingUpFrame() {
  auto s = std::make_unique<NavState>();
  s->headingUpMode = true;
  s->hasUserPosition = true;
  s->userX = 204;
  s->userY = 204;
  return s;
}

static void testUserLandsOnTheAnchor() {
  auto s = headingUpFrame();
  for (float heading : {0.0f, 37.0f, 90.0f, 215.0f}) {
    MapView v;
    v.headingKnown = true;
    v.headingDeg = heading;
    const MapXf xf = makeXf(*s, v);
    int16_t x, y;
    xfApply(xf, 204, 204, x, y);
    CHECK(x == MAP_ANCHOR_X && y == MAP_ANCHOR_Y);
  }
}

static void expectAt(const MapXf& xf, int sx, int sy, int ex, int ey, int line) {
  int16_t x, y;
  xfApply(xf, static_cast<int16_t>(sx), static_cast<int16_t>(sy), x, y);
  checks++;
  if (x != ex || y != ey) {
    failures++;
    std::printf("FAIL line %d: (%d,%d) -> (%d,%d), expected (%d,%d)\n", line, sx, sy, x, y, ex, ey);
  }
}

static void testTravelDirectionPointsUp() {
  auto s = headingUpFrame();
  MapView v;
  v.headingKnown = true;
  const int a = 20;
  // Source canvas: x east, y south. A point 20 px ahead of the user must land 20 px above the anchor.
  v.headingDeg = 0.0f;    // north
  expectAt(makeXf(*s, v), 204, 204 - a, MAP_ANCHOR_X, MAP_ANCHOR_Y - a, __LINE__);
  v.headingDeg = 90.0f;   // east
  expectAt(makeXf(*s, v), 204 + a, 204, MAP_ANCHOR_X, MAP_ANCHOR_Y - a, __LINE__);
  v.headingDeg = 180.0f;  // south
  expectAt(makeXf(*s, v), 204, 204 + a, MAP_ANCHOR_X, MAP_ANCHOR_Y - a, __LINE__);
  v.headingDeg = 270.0f;  // west
  expectAt(makeXf(*s, v), 204 - a, 204, MAP_ANCHOR_X, MAP_ANCHOR_Y - a, __LINE__);
}

static void testRightOfTravelStaysOnTheRight() {
  auto s = headingUpFrame();
  MapView v;
  v.headingKnown = true;
  v.headingDeg = 90.0f;  // heading east: south of the user is on the right-hand side
  expectAt(makeXf(*s, v), 204, 204 + 20, MAP_ANCHOR_X + 20, MAP_ANCHOR_Y, __LINE__);
  v.headingDeg = 0.0f;
  expectAt(makeXf(*s, v), 204 + 20, 204, MAP_ANCHOR_X + 20, MAP_ANCHOR_Y, __LINE__);
}

static void testRotationPreservesDistances() {
  auto s = headingUpFrame();
  MapView v;
  v.headingKnown = true;
  for (float heading = 0.0f; heading < 360.0f; heading += 13.0f) {
    v.headingDeg = heading;
    const MapXf xf = makeXf(*s, v);
    int16_t x0, y0, x1, y1;
    xfApply(xf, 150, 120, x0, y0);
    xfApply(xf, 260, 300, x1, y1);
    const float before = std::hypot(110.0f, 180.0f);
    const float after = std::hypot(static_cast<float>(x1 - x0), static_cast<float>(y1 - y0));
    CHECK(near(before, after, 1.5f));  // integer rounding only
  }
}

static void testVisibleCornersAreCoveredAtEveryHeading() {
  // Every screen corner, mapped back to canvas pixels, must lie inside the clip circle (radius 203 about 204).
  const float radius = 203.0f;
  const float corners[4][2] = {{0, 0}, {239, 0}, {0, 231}, {239, 231}};
  for (float heading = 0.0f; heading < 360.0f; heading += 1.0f) {
    const float rad = heading * DEG_TO_RAD;
    const float c = std::cos(rad), s = std::sin(rad);
    for (const auto& corner : corners) {
      const float u = corner[0] - MAP_ANCHOR_X;      // right of travel
      const float f = MAP_ANCHOR_Y - corner[1];      // ahead of travel
      // Inverse of xfApply: dx = u*c + f*s, dy = f*(-c)... derived from forward map.
      const float dx = u * c + f * s;
      const float dy = u * s - f * c;
      CHECK(std::hypot(dx, dy) <= radius);
    }
  }
}

static void testFramesWithoutBearingValidAreDrawnAsReceived() {
  auto s = std::make_unique<NavState>();
  s->hasUserPosition = true;
  s->userX = 120;
  s->userY = 116;
  MapView v;
  v.headingKnown = true;
  v.headingDeg = 123.0f;
  const MapXf xf = makeXf(*s, v);
  expectAt(xf, 50, 60, 50, 60, __LINE__);
  expectAt(xf, 120, 116, 120, 116, __LINE__);
}

static void testUnknownHeadingKeepsMapNorthUpAroundTheAnchor() {
  auto s = headingUpFrame();
  MapView v;  // heading unknown
  expectAt(makeXf(*s, v), 204 + 30, 204 - 40, MAP_ANCHOR_X + 30, MAP_ANCHOR_Y - 40, __LINE__);
}

static void testManeuverNamesMatchTheApp() {
  // Codes produced by the Android ManeuverIcon object.
  CHECK(maneuverFromName("depart") == MAN_DEPART);
  CHECK(maneuverFromName("arrive") == MAN_ARRIVE);
  CHECK(maneuverFromName("straight") == MAN_STRAIGHT);
  CHECK(maneuverFromName("left") == MAN_LEFT);
  CHECK(maneuverFromName("right") == MAN_RIGHT);
  CHECK(maneuverFromName("slight_left") == MAN_SLIGHT_LEFT);
  CHECK(maneuverFromName("slight_right") == MAN_SLIGHT_RIGHT);
  CHECK(maneuverFromName("sharp_left") == MAN_SHARP_LEFT);
  CHECK(maneuverFromName("sharp_right") == MAN_SHARP_RIGHT);
  CHECK(maneuverFromName("uturn") == MAN_UTURN);
  CHECK(maneuverFromName("roundabout") == MAN_ROUNDABOUT);
  CHECK(maneuverFromName("") == MAN_NONE);
  CHECK(maneuverFromName("teleport") == MAN_NONE);
  CHECK(maneuverFromName(nullptr) == MAN_NONE);
}

int main() {
  testManeuverNamesMatchTheApp();
  testWrap();
  testUnknownUntilValidSample();
  testCrossingNorthTakesTheShortWay();
  testNoiseInsideDeadbandIsIgnored();
  testSlowDriftEventuallyFollows();
  testStationaryHoldsHeadingAndNeverRedraws();
  testRotationRateIsCapped();
  testRedrawOnlyWhenAngleMovedEnough();
  testResetForgetsHeading();
  testUserLandsOnTheAnchor();
  testTravelDirectionPointsUp();
  testRightOfTravelStaysOnTheRight();
  testRotationPreservesDistances();
  testVisibleCornersAreCoveredAtEveryHeading();
  testFramesWithoutBearingValidAreDrawnAsReceived();
  testUnknownHeadingKeepsMapNorthUpAroundTheAnchor();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
