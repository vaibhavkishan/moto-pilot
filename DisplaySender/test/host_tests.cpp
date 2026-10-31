// Host-side tests for the pure CYD logic (no hardware). Build and run:
//   python -m ziglang c++ -std=c++17 -I DisplaySender DisplaySender/test/host_tests.cpp DisplaySender/nav_heading.cpp DisplaySender/motion_predictor.cpp DisplaySender/zoom_controller.cpp -o host_tests.exe
// Any C++17 compiler works. Arduino ignores this folder.
#include <cmath>
#include <cstdio>
#include <memory>

#include "nav_heading.h"
#include "motion_predictor.h"
#include "zoom_controller.h"
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
static bool near_f(float a, float b) { return std::fabs(a - b) <= 1e-3f; }

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
  CHECK(near(HeadingFilter::wrap180(f.display() - 2.0f), 0.0f, 1.0f));  // settles on the new course
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
  for (uint32_t t = 50; t <= 8000; t += 50) {
    f.tick(t);
    const float step = std::fabs(HeadingFilter::wrap180(f.display() - previous));
    if (step > maxStep) maxStep = step;
    previous = f.display();
  }
  CHECK(maxStep <= 220.0f * 0.05f + 0.01f);
  CHECK(near(HeadingFilter::wrap180(f.display() - 161.5f), 0.0f, 1.0f));  // 0 + 170 * 0.95
}

static void testRedrawOnlyWhenAngleMovedEnough() {
  HeadingFilter f;
  f.onSample(90.0f, true);
  f.tick(0);
  f.markDrawn();
  f.onSample(100.0f, true);  // real turn, target 99
  int redraws = 0;
  for (uint32_t t = 50; t <= 3000; t += 50) {
    if (f.tick(t)) {
      redraws++;
      f.markDrawn();
    }
  }
  CHECK(redraws >= 10 && redraws <= 59);  // rotates smoothly while turning, never more than once per tick
  CHECK(near(f.display(), 99.0f, 0.5f));
}

static void testSteadyCourseWithNoiseDoesNotWobble() {
  HeadingFilter f;
  uint32_t t = 0;
  float lo = 1000.0f, hi = -1000.0f;
  f.onSample(90.0f, true, 0);  // settled on the course; the noise then stays inside the dead band
  for (int i = 0; i < 40; i++) {
    f.onSample(90.0f + ((i * 7) % 5 - 2) * 0.8f, true, t);  // +-1.6 degrees of GPS noise at 2 Hz
    for (uint32_t end = t + 500; t < end; t += 20) {
      f.tick(t);
      if (i > 4) { lo = std::fmin(lo, f.display()); hi = std::fmax(hi, f.display()); }
    }
  }
  CHECK(hi - lo < 0.35f);
}

static void testGyroFusedHeadingTracksATurnClosely() {
  HeadingFilter f;
  uint32_t t = 0;
  float worst = 0.0f;
  for (uint32_t ms = 0; ms <= 8000; ms += 20) {
    const float u = ms < 2000 ? 0.0f : (ms > 5000 ? 1.0f : (ms - 2000) / 3000.0f);
    const float truth = 90.0f * (u * u * (3.0f - 2.0f * u));
    if (ms % 100 == 0) f.onSample(truth, true, ms);  // 10 Hz fused samples
    f.tick(ms);
    if (ms > 1000) worst = std::fmax(worst, std::fabs(HeadingFilter::wrap180(f.display() - truth)));
    t = ms;
  }
  (void)t;
  CHECK(worst < 4.0f);
}

static void testTurnRateKeepsTheAngleMovingBetweenSamples() {
  HeadingFilter f;
  uint32_t t = 0;
  float bearing = 0.0f;
  f.onSample(bearing, true, t);
  f.tick(t);
  f.markDrawn();
  // 30 deg/s turn sampled at 2 Hz, then the samples stop for half a second.
  for (int i = 1; i <= 4; i++) {
    bearing += 15.0f;
    const uint32_t sampleAt = static_cast<uint32_t>(i) * 500;
    for (; t < sampleAt; t += 20) f.tick(t);
    f.onSample(bearing, true, sampleAt);
  }
  const float atLastSample = f.display();
  float previous = atLastSample;
  float maxStep = 0.0f;
  for (t = 2000; t < 2500; t += 20) {
    f.tick(t);
    maxStep = std::fmax(maxStep, std::fabs(HeadingFilter::wrap180(f.display() - previous)));
    previous = f.display();
  }
  CHECK(HeadingFilter::wrap180(f.display() - atLastSample) > 4.0f);  // kept turning with no new sample
  CHECK(maxStep < 1.5f);                                             // in small steps, not one jump
}

static void testSteadyHeadingIsNotExtrapolated() {
  HeadingFilter f;
  uint32_t t = 0;
  f.onSample(90.0f, true, 0);  // settled on the course; GPS noise then stays inside the dead band
  for (int i = 0; i < 6; i++) {
    f.onSample(90.0f + (i % 2 ? 1.5f : -1.5f), true, t);  // GPS noise around a straight course
    for (uint32_t end = t + 500; t < end; t += 20) f.tick(t);
  }
  CHECK(near(f.display(), 90.0f, 0.5f));
}

static const double kLatPerMetre = 1.0 / 111320.0;

static void testMotionAdvancesAlongTheHeading() {
  MotionPredictor m;
  m.onFrame(0.0, 0.0, 10.0f, 0.0f, true, 0.5f, 0, 0);
  for (uint32_t t = 20; t <= 1000; t += 20) m.tick(t, 0.0f);
  CHECK(near(m.offsetY(), -5.0f, 0.05f));  // 10 m/s for 1 s at 0.5 px/m, north is up the canvas
  CHECK(near(m.offsetX(), 0.0f, 0.01f));
  MotionPredictor east;
  east.onFrame(0.0, 0.0, 10.0f, 90.0f, true, 0.5f, 0, 0);
  for (uint32_t t = 20; t <= 1000; t += 20) east.tick(t, 90.0f);
  CHECK(near(east.offsetX(), 5.0f, 0.05f));
}

static void testFixAgeIsAddedImmediately() {
  MotionPredictor m;
  m.onFrame(0.0, 0.0, 10.0f, 0.0f, true, 0.5f, 200, 0);
  CHECK(near(m.offsetY(), -1.0f, 0.01f));  // 200 ms old at 10 m/s
}

// World position of the drawn user, in metres north, from the frame centre and the offset.
static double drawnNorth(const MotionPredictor& m, double frameLat, float pxPerM) {
  return frameLat / kLatPerMetre - m.offsetY() / pxPerM;
}

static void testNewFrameNeverMakesTheMapJump() {
  MotionPredictor m;
  const float pxPerM = 0.5f;
  double lat = 0.0;
  uint32_t t = 0;
  m.onFrame(lat, 0.0, 10.0f, 0.0f, true, pxPerM, 0, t);
  double worstJump = 0.0;
  for (int frame = 1; frame <= 6; frame++) {
    for (uint32_t end = t + 500; t < end; t += 20) m.tick(t + 20, 0.0f);
    const double before = drawnNorth(m, lat, pxPerM);
    lat += 5.0 * kLatPerMetre;  // the real fix agrees with the prediction
    m.onFrame(lat, 0.0, 10.0f, 0.0f, true, pxPerM, 0, t);
    worstJump = std::fmax(worstJump, std::fabs(drawnNorth(m, lat, pxPerM) - before));
  }
  CHECK(worstJump < 0.05);  // metres
}

static void testGpsCorrectionFadesInsteadOfSnapping() {
  MotionPredictor m;
  const float pxPerM = 0.5f;
  m.onFrame(0.0, 0.0, 10.0f, 0.0f, true, pxPerM, 0, 0);
  uint32_t t = 0;
  for (; t < 500; t += 20) m.tick(t + 20, 0.0f);
  const double before = drawnNorth(m, 0.0, pxPerM);
  const double lat = 3.0 * kLatPerMetre;  // the fix is 2 m behind the prediction
  m.onFrame(lat, 0.0, 10.0f, 0.0f, true, pxPerM, 0, t);
  CHECK(std::fabs(drawnNorth(m, lat, pxPerM) - before) < 0.05);  // no jump on arrival
  for (int i = 0; i < 100; i++) { t += 20; m.tick(t, 0.0f); }
  CHECK(std::fabs(drawnNorth(m, lat, pxPerM) - (3.0 + 10.0 * 2.0)) < 0.5);  // settled on fix + travel
}

static void testLightFixesKeepTheMapContinuous() {
  MotionPredictor m;
  const float px = 0.5f;
  const double centre = 0.0;
  m.onFrame(centre, 0.0, 10.0f, 0.0f, true, px, 0, 0);
  double trueNorth = 0.0;  // metres
  double worstJump = 0.0;
  double worstError = 0.0;
  uint32_t t = 0;
  for (int fix = 1; fix <= 30; fix++) {  // 3 s of 10 Hz fixes with no new canvas
    for (int k = 0; k < 5; k++) { t += 20; m.tick(t, 0.0f); }
    trueNorth += 1.0;  // 10 m/s for 100 ms
    const double before = centre / kLatPerMetre - m.offsetY() / px;
    m.onFix(trueNorth * kLatPerMetre, 0.0, 10.0f, 0.0f, true, 0, t);
    const double after = centre / kLatPerMetre - m.offsetY() / px;
    worstJump = std::fmax(worstJump, std::fabs(after - before));
    worstError = std::fmax(worstError, std::fabs(after - trueNorth));
  }
  CHECK(worstJump < 0.05);   // no jump when a fix arrives
  CHECK(worstError < 0.6);   // and it stays within the distance travelled between fixes
}

static void testLightFixCorrectionFades() {
  MotionPredictor m;
  const float px = 0.5f;
  m.onFrame(0.0, 0.0, 10.0f, 0.0f, true, px, 0, 0);
  uint32_t t = 0;
  for (int i = 0; i < 5; i++) { t += 20; m.tick(t, 0.0f); }  // drawn 1 m ahead
  m.onFix(3.0 * kLatPerMetre, 0.0, 10.0f, 0.0f, true, 0, t);   // the fix says 3 m: 2 m ahead of the drawing
  const double right = 3.0;
  for (int i = 0; i < 150; i++) { t += 20; m.tick(t, 0.0f); }  // 3 s later
  const double drawn = -m.offsetY() / px;
  CHECK(std::fabs(drawn - (right + 10.0 * 1.98)) < 0.3);      // fix plus the travel since it (prediction capped at 2 s)
}

static void testFixBeforeAnyFrameIsIgnored() {
  MotionPredictor m;
  m.onFix(1.0, 1.0, 10.0f, 0.0f, true, 0, 0);
  CHECK(near_f(m.offsetX(), 0.0f) && near_f(m.offsetY(), 0.0f));
}

static void testTeleportSnapsAndStationaryDoesNotMove() {
  MotionPredictor m;
  m.onFrame(0.0, 0.0, 10.0f, 0.0f, true, 0.5f, 0, 0);
  m.onFrame(1000.0 * kLatPerMetre, 0.0, 10.0f, 0.0f, true, 0.5f, 0, 500);  // 1 km away
  CHECK(near(m.offsetY(), 0.0f, 1e-3f));
  MotionPredictor still;
  still.onFrame(0.0, 0.0, 0.2f, 0.0f, true, 0.5f, 0, 0);
  MotionPredictor weak;
  weak.onFrame(0.0, 0.0, 10.0f, 0.0f, false, 0.5f, 0, 0);
  for (uint32_t t = 20; t <= 1000; t += 20) { still.tick(t, 0.0f); weak.tick(t, 0.0f); }
  CHECK(near(still.offsetY(), 0.0f, 1e-3f));
  CHECK(near(weak.offsetY(), 0.0f, 1e-3f));
}

static void testPredictionStopsWhenFramesStop() {
  MotionPredictor m;
  m.onFrame(0.0, 0.0, 10.0f, 0.0f, true, 0.5f, 0, 0);
  for (uint32_t t = 20; t <= 6000; t += 20) m.tick(t, 0.0f);
  CHECK(near(m.offsetY(), -10.0f, 0.2f));  // capped at 2 s of travel
}

static std::unique_ptr<NavState> headingUpFrame();

static void testZoomStrengthAndLeadComeFromTheSettings() {
  const float px = 0.29f;
  CHECK(near_f(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, px, 1.0f, 8.0f), 1.0f));  // switched off
  CHECK(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, px, 2.0f, 8.0f) <= 2.0f + 1e-4f);
  CHECK(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, px, 5.0f, 8.0f) > 4.5f);
  CHECK(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, 0.145f, 5.0f, 8.0f) <= 5.0f + 1e-4f);
  CHECK(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, 0.58f, 5.0f, 8.0f) <= 2.5f / 0.58f + 1e-3f);  // never closer than 2.5 px/m
  // A longer lead starts the zoom earlier at the same distance.
  const float shortLead = ZoomController::targetFor(MAN_RIGHT, 120.0f, 12.0f, px, 3.5f, 4.0f);
  const float longLead = ZoomController::targetFor(MAN_RIGHT, 120.0f, 12.0f, px, 3.5f, 14.0f);
  CHECK(near_f(shortLead, 1.0f) && longLead > 1.3f);
}

static void testZoomGrowsAsTheTurnGetsCloser() {
  const float px = 0.29f;  // 400 m map radius
  const float far = ZoomController::targetFor(MAN_RIGHT, 600.0f, 12.0f, px, 3.5f, 8.0f);
  const float mid = ZoomController::targetFor(MAN_RIGHT, 80.0f, 12.0f, px, 3.5f, 8.0f);
  const float near = ZoomController::targetFor(MAN_RIGHT, 30.0f, 12.0f, px, 3.5f, 8.0f);
  CHECK(near_f(far, 1.0f));
  CHECK(mid > 1.2f && mid < near);
  CHECK(near > 3.0f && near <= 3.5f);
  float previous = 0.0f;
  for (float d = 400.0f; d >= 0.0f; d -= 10.0f) {
    const float z = ZoomController::targetFor(MAN_LEFT, d, 12.0f, px, 3.5f, 8.0f);
    CHECK(z >= previous - 1e-5f);  // never zooms out while the turn is getting closer
    previous = z;
  }
}

static void testZoomOnlyForTurnsAndOnlyWhenTheMapIsFar() {
  CHECK(near_f(ZoomController::targetFor(MAN_STRAIGHT, 20.0f, 12.0f, 0.29f, 3.5f, 8.0f), 1.0f));
  CHECK(near_f(ZoomController::targetFor(MAN_DEPART, 20.0f, 12.0f, 0.29f, 3.5f, 8.0f), 1.0f));
  CHECK(near_f(ZoomController::targetFor(MAN_NONE, 20.0f, 12.0f, 0.29f, 3.5f, 8.0f), 1.0f));
  CHECK(ZoomController::targetFor(MAN_ROUNDABOUT, 20.0f, 12.0f, 0.29f, 3.5f, 8.0f) > 3.0f);
  CHECK(ZoomController::targetFor(MAN_ARRIVE, 20.0f, 12.0f, 0.29f, 3.5f, 8.0f) > 3.0f);
  CHECK(near_f(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, 0.0f, 3.5f, 8.0f), 1.0f));      // unknown scale
  CHECK(near_f(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, 2.4f, 3.5f, 8.0f), 1.0f));      // already close
  CHECK(ZoomController::targetFor(MAN_RIGHT, 20.0f, 12.0f, 0.145f, 3.5f, 8.0f) <= 3.5f);         // capped
}

static void testZoomStartsEarlierWhenFast() {
  const float slow = ZoomController::targetFor(MAN_RIGHT, 200.0f, 5.0f, 0.29f, 3.5f, 8.0f);
  const float fast = ZoomController::targetFor(MAN_RIGHT, 200.0f, 30.0f, 0.29f, 3.5f, 8.0f);
  CHECK(fast > slow);
}

static void testZoomMovesSmoothlyInAndBackOut() {
  ZoomController z;
  uint32_t t = 0;
  z.tick(t);
  z.setTarget(true, MAN_RIGHT, 30.0f, 12.0f, 0.29f, 3.5f, 8.0f);
  float previous = z.zoom();
  float maxStep = 0.0f;
  for (int i = 0; i < 150; i++) {  // 3 s at 50 fps
    t += 20;
    z.tick(t);
    maxStep = std::fmax(maxStep, std::fabs(z.zoom() - previous));
    previous = z.zoom();
  }
  CHECK(z.zoom() > 3.0f);
  CHECK(maxStep <= 1.5f * 0.02f + 1e-4f);  // rate limited, no jump
  z.setTarget(true, MAN_STRAIGHT, 800.0f, 12.0f, 0.29f, 3.5f, 8.0f);  // the turn is taken, the next one is far
  CHECK(z.zoom() > 3.0f);
  for (int i = 0; i < 25; i++) { t += 20; z.tick(t); }
  CHECK(z.zoom() > 1.5f);  // still easing out half a second later
  for (int i = 0; i < 300; i++) { t += 20; z.tick(t); }
  CHECK(near_f(z.zoom(), 1.0f));
}

static void testZoomEasesBackWhenOffRoute() {
  ZoomController z;
  uint32_t t = 0;
  z.tick(t);
  z.setTarget(true, MAN_RIGHT, 30.0f, 12.0f, 0.29f, 3.5f, 8.0f);
  for (int i = 0; i < 150; i++) { t += 20; z.tick(t); }
  z.setTarget(false, MAN_RIGHT, 30.0f, 12.0f, 0.29f, 3.5f, 8.0f);
  for (int i = 0; i < 400; i++) { t += 20; z.tick(t); }
  CHECK(near_f(z.zoom(), 1.0f));
  z.reset();
  CHECK(near_f(z.zoom(), 1.0f));
}

static void testZoomScalesDistancesAroundTheUser() {
  auto s = headingUpFrame();
  MapView v;
  v.headingKnown = true;
  v.headingDeg = 0.0f;
  v.zoom = 2.0f;
  int16_t x, y;
  xfApply(makeXf(*s, v), 204, 204 - 10, x, y);  // 10 canvas px ahead of the user
  CHECK(x == MAP_ANCHOR_X && y == MAP_ANCHOR_Y - 20);
  xfApply(makeXf(*s, v), 204, 204, x, y);
  CHECK(x == MAP_ANCHOR_X && y == MAP_ANCHOR_Y);  // the user does not move
}

static void testOffsetMovesTheGeometryUnderTheAnchor() {
  auto s = headingUpFrame();
  MapView v;
  v.headingKnown = true;
  v.headingDeg = 0.0f;
  v.offsetY = -10.0f;  // the user has moved 10 px north since the frame
  int16_t x, y;
  xfApply(makeXf(*s, v), 204, 204 - 10, x, y);  // a point that was 10 px north of the frame centre
  CHECK(x == MAP_ANCHOR_X && y == MAP_ANCHOR_Y);  // is now exactly under the user
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
  testSteadyCourseWithNoiseDoesNotWobble();
  testGyroFusedHeadingTracksATurnClosely();
  testTurnRateKeepsTheAngleMovingBetweenSamples();
  testSteadyHeadingIsNotExtrapolated();
  testResetForgetsHeading();
  testUserLandsOnTheAnchor();
  testTravelDirectionPointsUp();
  testRightOfTravelStaysOnTheRight();
  testRotationPreservesDistances();
  testVisibleCornersAreCoveredAtEveryHeading();
  testFramesWithoutBearingValidAreDrawnAsReceived();
  testUnknownHeadingKeepsMapNorthUpAroundTheAnchor();
  testMotionAdvancesAlongTheHeading();
  testFixAgeIsAddedImmediately();
  testNewFrameNeverMakesTheMapJump();
  testGpsCorrectionFadesInsteadOfSnapping();
  testLightFixesKeepTheMapContinuous();
  testLightFixCorrectionFades();
  testFixBeforeAnyFrameIsIgnored();
  testTeleportSnapsAndStationaryDoesNotMove();
  testPredictionStopsWhenFramesStop();
  testOffsetMovesTheGeometryUnderTheAnchor();
  testZoomGrowsAsTheTurnGetsCloser();
  testZoomStrengthAndLeadComeFromTheSettings();
  testZoomOnlyForTurnsAndOnlyWhenTheMapIsFar();
  testZoomStartsEarlierWhenFast();
  testZoomMovesSmoothlyInAndBackOut();
  testZoomEasesBackWhenOffRoute();
  testZoomScalesDistancesAroundTheUser();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
