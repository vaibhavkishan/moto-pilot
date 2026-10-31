/**
 * MotoPilot — ESP32-2432S028 (CYD)
 *
 * BLE UART (Nordic NUS) server receives JSON navigation updates from the
 * Android app and renders a vector map (route + position + maneuver overlay).
 *
 * IMPORTANT: Never call TFT / ArduinoJson from NimBLE callbacks — they run on
 * a small BLE task stack and will reboot the CYD. Callbacks only queue bytes /
 * flags; loop() does drawing and parsing.
 *
 * Libraries:
 *   - TFT_eSPI (Bodmer)
 *   - NimBLE-Arduino (h2zero) v2.x
 *   - ArduinoJson (Benoit Blanchon) v7+
 */

#include "config.h"

#include <NimBLEDevice.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "nav_types.h"
#include "nav_heading.h"
#include "motion_predictor.h"
#include "zoom_controller.h"
#include "map_renderer.h"
#include "nav_protocol.h"
#include "loading_screen.h"
#include "logo.h"
#include "scene.h"
#include "maps_theme.h"
#include "touch_cyd.h"
#include "utf8_text.h"
#include <ArduinoJson.h>
#include <string.h>

TFT_eSPI tft;
MapRenderer mapRenderer(tft);

static volatile bool clientConnected = false;
static volatile bool awaitingFirstNav = false;
static volatile bool uiShowLoading = false;
static volatile bool uiShowWaiting = false;
static volatile bool sendOkPending = false;

static uint32_t navUpdatesReceived = 0;
static uint32_t lastStatsMillis = 0;
static uint16_t updatesPerSecond = 0;
static uint16_t motionUpdatesPerSecond = 0;

static NavState lastNav;
static bool hasNav = false;

// Rendering is decoupled from BLE parsing: frames only update lastNav, loop() draws the latest.
// While riding, loop() redraws as fast as the panel allows: the map glides between GPS frames.
static HeadingFilter headingFilter;
static MotionPredictor motion;
static ZoomController zoom;
static bool navDirty = false;
static bool benchActive = false;
static uint16_t rendersPerSecond = 0;
static uint32_t renderUsSum = 0;
static uint32_t renderUsMax = 0;
static bool showingWaiting = true;
static bool showingArrived = false;
static bool touchWasDown = false;
static uint32_t lastThemeToggleMs = 0;

static const size_t LINE_BUF_SIZE = 8192;
static char lineBuf[LINE_BUF_SIZE];
static size_t lineLen = 0;
static bool discardLine = false;

// Byte queue filled in BLE write callback, drained in loop()
static const size_t RX_QUEUE_SIZE = 8192;
static uint8_t rxQueue[RX_QUEUE_SIZE];
static volatile size_t rxHead = 0;
static volatile size_t rxTail = 0;
static bool rxOverflow = false;

static NimBLEServer* bleServer = nullptr;
static NimBLECharacteristic* txCharacteristic = nullptr;

bool initDisplay();
bool initBle();
void showStatusScreen(const char* title, const char* line2 = nullptr, const char* line3 = nullptr);
void showWaitingForAppScreen();
void processTextMessage(const char* payload, size_t length);
void drainRxQueue();
void appendRxByte(char c);
void updateStatsCounter();
void handleUiFlags();
void pollThemeSwitch();
bool renderTick();
void applyNavFrame(const NavState& state);
void applyMotionUpdate(const MotionUpdate& update);

// The arrival screen matches the style of the last map that was shown.
static void showArrivalScreen() {
  if (lastNav.style == STYLE_MINIMAL) {
    drawArrivalScreenMinimal(tft, mapsThemeIsDark());
  } else {
    drawArrivalScreen(tft, mapsThemeIsDark());
  }
}
void handleSerialCommands();
void benchTick();
static void toggleBench(bool roundabout = false);
void redrawAfterThemeChange();

static bool rxQueuePush(uint8_t b) {
  const size_t next = (rxHead + 1) % RX_QUEUE_SIZE;
  if (next == rxTail) {
    rxOverflow = true;
    return false; // full
  }
  rxQueue[rxHead] = b;
  rxHead = next;
  return true;
}

static bool rxQueuePop(uint8_t& b) {
  if (rxTail == rxHead) {
    return false;
  }
  b = rxQueue[rxTail];
  rxTail = (rxTail + 1) % RX_QUEUE_SIZE;
  return true;
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*connInfo*/) override {
    clientConnected = true;
    awaitingFirstNav = true;
    navUpdatesReceived = 0;
    updatesPerSecond = 0;
    lastStatsMillis = millis();
    lineLen = 0;
    discardLine = false;
    rxOverflow = false;
    hasNav = false;
    showingWaiting = false;
    uiShowLoading = true;
    sendOkPending = true;
    // Do NOT draw TFT or notify heavily here — defer to loop()
  }

  void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*connInfo*/, int /*reason*/) override {
    clientConnected = false;
    awaitingFirstNav = false;
    navUpdatesReceived = 0;
    lineLen = 0;
    discardLine = false;
    rxOverflow = false;
    hasNav = false;
    rxHead = 0;
    rxTail = 0;
    uiShowWaiting = true;
    // Advertising restart deferred to loop() — safer than here
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& /*connInfo*/) override {
    const NimBLEAttValue& value = characteristic->getValue();
    const size_t len = value.length();
    const uint8_t* data = value.data();
    for (size_t i = 0; i < len; i++) {
      if (!rxQueuePush(data[i])) {
        break; // drop overflow rather than crash
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== MotoPilot CYD (BLE) ==="));

  mapsThemeInit();

  if (!initDisplay()) {
    Serial.println(F("Failed to initialize display."));
    while (true) { delay(1000); }
  }

  if (!touchInit()) {
    Serial.println(F("Touch init failed — theme switch disabled"));
  }

  showStatusScreen("MotoPilot", "Starting BLE...");

  if (!initBle()) {
    showStatusScreen("BLE Error", "Restart the board.");
    while (true) { delay(1000); }
  }

  mapRenderer.begin();

  showWaitingForAppScreen();
  showingWaiting = true;
}

void loop() {
  handleUiFlags();
  drainRxQueue();
  pollThemeSwitch();
  handleSerialCommands();
  benchTick();
  const bool rendered = renderTick();

  if (clientConnected && awaitingFirstNav) {
    updateMapLoadingAnimation(tft, millis());
  }

  updateStatsCounter();
  if (rendered) {
    yield();
  } else {
    delay(2);
  }
}

void handleUiFlags() {
  if (uiShowLoading) {
    uiShowLoading = false;
    benchActive = false;  // a real phone connected: stop the demo ride
    showingWaiting = false;
    showingArrived = false;
    showMapLoadingScreen(tft);
  }

  if (uiShowWaiting) {
    uiShowWaiting = false;
    showingWaiting = true;
    showingArrived = false;
    showWaitingForAppScreen();
    if (bleServer != nullptr) {
      bleServer->startAdvertising();
    }
  }

  if (sendOkPending && clientConnected && txCharacteristic != nullptr) {
    sendOkPending = false;
    txCharacteristic->setValue("OK\n");
    txCharacteristic->notify();
  }
}

void redrawAfterThemeChange() {
  if (hasNav && (clientConnected || benchActive)) {
    mapRenderer.invalidate();
    navDirty = true;
    showingWaiting = false;
  } else if ((clientConnected || benchActive) && showingArrived) {
    showArrivalScreen();
    showingWaiting = false;
  } else if (clientConnected && awaitingFirstNav) {
    showMapLoadingScreen(tft);
    showingWaiting = false;
  } else {
    showWaitingForAppScreen();
    showingWaiting = true;
  }
}

void pollThemeSwitch() {
  static uint32_t touchStartMs = 0;
  static uint16_t touchStartY = 0;
  static bool holdHandled = false;
  uint16_t x = 0;
  uint16_t y = 0;
  const bool down = touchRead(x, y);
  const uint32_t now = millis();

  if (down && !touchWasDown) {
    touchStartMs = now;
    touchStartY = y;
    holdHandled = false;
    if (benchActive && y < MAP_AREA_H) {
      toggleBench();  // a tap on the map ends the demo ride
      holdHandled = true;
    }
  }
  if (down && !holdHandled && !benchActive && showingWaiting && touchStartY < 90 && now - touchStartMs > 1000) {
    toggleBench();  // holding the logo for a second starts the demo ride
    holdHandled = true;
  }

  if (down && !touchWasDown && !holdHandled) {
    const bool hit = MapRenderer::themeSwitchHit(x, y);
    Serial.printf("tap %u,%u hit=%d\n", x, y, hit ? 1 : 0);
    if (hit && (now - lastThemeToggleMs) > 350) {
      const bool dark = mapsThemeToggle();
      lastThemeToggleMs = now;
      Serial.printf("theme -> %s\n", dark ? "dark" : "light");
      redrawAfterThemeChange();
    }
  }
  touchWasDown = down;
}

bool initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.writecommand(0x26);
  tft.writedata(2);
  delay(120);
  tft.writecommand(0x26);
  tft.writedata(1);

  tft.setRotation(0);
  tft.fillScreen(mapsColLand());
  return true;
}

bool initBle() {
  // Keep NimBLE lean — CYD has limited RAM with TFT_eSPI
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  NimBLEDevice::setMTU(BLE_MTU);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  bleServer->advertiseOnDisconnect(false); // we restart advertising from loop()

  NimBLEService* service = bleServer->createService(NUS_SERVICE_UUID);
  NimBLECharacteristic* rx = service->createCharacteristic(
      NUS_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(new RxCallbacks());

  txCharacteristic = service->createCharacteristic(
      NUS_TX_UUID,
      NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
  );

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(NUS_SERVICE_UUID);
  advertising->start();

  Serial.printf("BLE advertising as \"%s\"\n", BLE_DEVICE_NAME);
  return true;
}

void showStatusScreen(const char* title, const char* line2, const char* line3) {
  drawStatusScreen(tft, title, line2, line3);
}

void showWaitingForAppScreen() {
  drawHomeScreen(tft, mapsThemeIsDark(), false, BLE_DEVICE_NAME);
}

void appendRxByte(char c) {
  if (c == '\n' || c == '\r') {
    if (lineLen > 0 && !discardLine) {
      lineBuf[lineLen] = '\0';
      processTextMessage(lineBuf, lineLen);
    }
    lineLen = 0;
    discardLine = false;
    return;
  }
  if (discardLine) {
    return;
  }
  if (lineLen + 1 >= LINE_BUF_SIZE) {
    Serial.println(F("BLE line buffer overflow — reset"));
    lineLen = 0;
    discardLine = true;
    return;
  }
  lineBuf[lineLen++] = c;
}

void drainRxQueue() {
  if (rxOverflow) {
    rxTail = rxHead;
    rxOverflow = false;
    lineLen = 0;
    discardLine = true;
    Serial.println(F("BLE overflow: discarded incomplete frame"));
  }
  uint8_t b;
  for (int i = 0; i < 1024; i++) {
    if (!rxQueuePop(b)) {
      break;
    }
    appendRxByte(static_cast<char>(b));
  }
}

void processTextMessage(const char* payload, size_t length) {
  if (isLoadingJson(payload, length)) {
    awaitingFirstNav = true;
    hasNav = false;
    showingWaiting = false;
    showingArrived = false;
    motion.reset();
    showMapLoadingScreen(tft);
    return;
  }

  if (isArrivedJson(payload, length)) {
    awaitingFirstNav = false;
    hasNav = false;
    showingWaiting = false;
    showingArrived = true;
    motion.reset();
    showArrivalScreen();
    return;
  }

  if (isMotionJson(payload, length)) {
    MotionUpdate update;
    if (parseMotionJson(payload, length, update)) {
      applyMotionUpdate(update);
    }
    return;
  }

  NavState state;
  if (!parseNavJson(payload, length, state)) {
    return;
  }
  applyNavFrame(state);
}

// A light update between full frames: the newest heading and fix move the map without a new canvas.
void applyMotionUpdate(const MotionUpdate& update) {
  if (!hasNav || !lastNav.headingUpMode) {
    return;
  }
  const uint32_t now = millis();
  headingFilter.onSample(update.bearing, update.bearingValid, now);
  motion.onFix(update.lat, update.lon, update.speedMps, update.bearing, update.bearingValid && !update.gpsWeak,
               update.ageMs, now);
  lastNav.speedMps = update.speedMps;
  motionUpdatesPerSecond++;
}

void applyNavFrame(const NavState& state) {
  const uint32_t now = millis();
  if (!hasNav) {
    // First frame after connect/loading: the screen holds something else and the heading is stale.
    headingFilter.reset();
    motion.reset();
    zoom.reset();
    mapRenderer.invalidate();
  }
  // Frames without bearing_valid give no way to tell a real course apart.
  const bool courseValid = state.headingUpMode ? state.bearingValid : true;
  headingFilter.onSample(state.bearing, courseValid, now);
  motion.onFrame(state.lat, state.lon, state.speedMps, state.bearing,
                 state.headingUpMode && state.bearingValid && !state.gpsWeak, state.pxPerM, state.ageMs, now);

  lastNav = state;
  hasNav = true;
  showingWaiting = false;
  showingArrived = false;
  navDirty = true;
  awaitingFirstNav = false;
  navUpdatesReceived++;
  updatesPerSecond++;
}

bool renderTick() {
  if (!hasNav || (!clientConnected && !benchActive)) {
    return false;
  }
  const uint32_t now = millis();
  const bool headingMoved = headingFilter.tick(now);
  const float heading = headingFilter.known() ? headingFilter.display() : 0.0f;
  const bool moved = motion.tick(now, heading);
  // Classic style only: zoom in toward the next turn and back out after it.
  const bool zoomWanted = lastNav.style == STYLE_CLASSIC && lastNav.headingUpMode && !lastNav.gpsWeak &&
                          !lastNav.offRoute && !lastNav.rerouting;
  if (lastNav.style != STYLE_CLASSIC) {
    zoom.reset();
  }
  zoom.setTarget(zoomWanted, lastNav.maneuver, lastNav.distanceM - motion.advancedMeters(), lastNav.speedMps,
                 lastNav.pxPerM, lastNav.zoomMax, lastNav.zoomLeadS);
  const bool zoomed = zoom.tick(now);
  if (!navDirty && !headingMoved && !moved && !zoomed) {
    return false;
  }
  navDirty = false;

  MapView view;
  view.headingKnown = headingFilter.known();
  view.headingDeg = heading;
  view.offsetX = motion.offsetX();
  view.offsetY = motion.offsetY();
  view.zoom = zoom.zoom();
  mapRenderer.draw(lastNav, view);
  headingFilter.markDrawn();

  const uint32_t us = mapRenderer.lastDrawUs();
  rendersPerSecond++;
  renderUsSum += us;
  if (us > renderUsMax) renderUsMax = us;
  return true;
}

void updateStatsCounter() {
  const uint32_t now = millis();
  if (now - lastStatsMillis >= 1000) {
    if ((clientConnected || benchActive) && navUpdatesReceived > 0) {
      Serial.printf("Nav updates/s: %u (+%u motion)  Total: %lu  Renders/s: %u  draw avg/max: %lu/%lu us  heap: %u (min %u)  sprite: %s\n",
                    updatesPerSecond,
                    motionUpdatesPerSecond,
                    static_cast<unsigned long>(navUpdatesReceived),
                    rendersPerSecond,
                    static_cast<unsigned long>(rendersPerSecond ? renderUsSum / rendersPerSecond : 0),
                    static_cast<unsigned long>(renderUsMax),
                    static_cast<unsigned>(ESP.getFreeHeap()),
                    static_cast<unsigned>(ESP.getMinFreeHeap()),
                    mapRenderer.usesSprite() ? "yes" : "no");
    }
    updatesPerSecond = 0;
    motionUpdatesPerSecond = 0;
    rendersPerSecond = 0;
    renderUsSum = 0;
    renderUsMax = 0;
    lastStatsMillis = now;
  }
}

// Demo ride: no phone needed. Start it with "b" over serial or by tapping the logo on the home
// screen; tap the map (or send "b") to stop. "m" switches the map style. It rides a route with
// right, left and right turns on a street grid, slowing for each turn, and ends on the arrival
// screen before starting over. Frames per second are printed once a second.
static bool benchMinimal = false;
static bool simRoundabout = false;  // the roundabout ride instead of the city ride
static float simDistance = 0.0f;  // metres ridden along the demo route
static float simSpeed = 6.0f;
static uint32_t simLastStepMs = 0;
static uint32_t simLastFrameMs = 0;
static uint32_t simArrivedAtMs = 0;
static uint32_t simLastMotionMs = 0;
static double simFixLat = 0.0;
static double simFixLon = 0.0;

static void toggleBench(bool roundabout) {
  if (!benchActive) {
    simRoundabout = roundabout;
  }
  benchActive = !benchActive;
  navUpdatesReceived = 0;
  hasNav = false;
  showingArrived = false;
  simDistance = 0.0f;
  simSpeed = 6.0f;
  simLastStepMs = 0;
  simLastFrameMs = 0;
  simArrivedAtMs = 0;
  simLastMotionMs = 0;
  Serial.println(benchActive ? F("bench: on") : F("bench: off"));
  if (!benchActive) {
    showWaitingForAppScreen();
    showingWaiting = true;
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == 'm') {
      benchMinimal = !benchMinimal;
      Serial.println(benchMinimal ? F("bench style: minimal") : F("bench style: classic"));
    } else if (c == 'b') {
      toggleBench(false);
    } else if (c == 'r') {
      if (benchActive) {
        toggleBench(false);
      }
      toggleBench(true);
    }
  }
}

namespace {

struct SimPoint {
  float x, y;  // metres east / north
};

// Corners and the end of the route; legs 0-3 follow the street grid (blocks of 120 m).
const SimPoint SIM_ROUTE[] = {{0, -240}, {0, 360}, {480, 360}, {480, 840}, {840, 840}};
const uint8_t SIM_MANEUVER[] = {MAN_DEPART, MAN_RIGHT, MAN_LEFT, MAN_RIGHT, MAN_ARRIVE};
const char* const SIM_INSTRUCTION[] = {"Start route", "Turn right", "Turn left", "Turn right", "Destination ahead"};
const char* const SIM_STREET[] = {"Maple Street", "Oak Avenue", "Cedar Road", "Harbour Drive"};
const int SIM_POINTS = 5;
const double SIM_GRID_M = 120.0;
const float SIM_PX_PER_M = 0.29f;  // the default 400 m map radius
const double METERS_PER_DEG = 111320.0;

float simLegLength(int leg) {
  return hypotf(SIM_ROUTE[leg + 1].x - SIM_ROUTE[leg].x, SIM_ROUTE[leg + 1].y - SIM_ROUTE[leg].y);
}

float simLegBearing(int leg) {
  float b = atan2f(SIM_ROUTE[leg + 1].x - SIM_ROUTE[leg].x, SIM_ROUTE[leg + 1].y - SIM_ROUTE[leg].y) / DEG_TO_RAD;
  return b < 0.0f ? b + 360.0f : b;
}

float simDistanceTo(int corner) {
  float d = 0.0f;
  for (int i = 0; i < corner; i++) d += simLegLength(i);
  return d;
}

// Leg the distance s falls on, and the point and heading there. Headings swing smoothly
// through each corner over 36 m, like a real bike turning.
void simLocate(float s, int& leg, float& px, float& py, float& heading) {
  leg = 0;
  float start = 0.0f;
  while (leg < SIM_POINTS - 2 && s > start + simLegLength(leg)) {
    start += simLegLength(leg);
    leg++;
  }
  const float along = s - start;
  const float length = simLegLength(leg);
  px = SIM_ROUTE[leg].x + (SIM_ROUTE[leg + 1].x - SIM_ROUTE[leg].x) * along / length;
  py = SIM_ROUTE[leg].y + (SIM_ROUTE[leg + 1].y - SIM_ROUTE[leg].y) * along / length;
  heading = simLegBearing(leg);
  for (int corner = 1; corner < SIM_POINTS - 1; corner++) {
    const float t = (s - (simDistanceTo(corner) - 18.0f)) / 36.0f;
    if (t > 0.0f && t < 1.0f) {
      const float before = simLegBearing(corner - 1);
      const float after = simLegBearing(corner);
      float delta = fmodf(after - before + 540.0f, 360.0f) - 180.0f;
      heading = fmodf(before + delta * (t * t * (3.0f - 2.0f * t)) + 360.0f, 360.0f);
    }
  }
}

void simAddRoutePoint(NavState& s, float wx, float wy, float userX, float userY) {
  if (s.routeCount >= NAV_MAX_ROUTE_POINTS) return;
  s.routeX[s.routeCount] = static_cast<int16_t>(204 + (wx - userX) * SIM_PX_PER_M);
  s.routeY[s.routeCount] = static_cast<int16_t>(204 - (wy - userY) * SIM_PX_PER_M);
  s.routeCount++;
}

// Roundabout demo: 500 m that enter a five-exit roundabout from the south and leave by the 4th exit.
const float RB_PX_PER_M = 0.77f;  // a 150 m map radius, so the roundabout is large enough to read
const float RB_RADIUS = 30.0f;
const float RB_TOTAL = 500.0f;
const float RB_APPROACH = 220.0f;
const float RB_EXIT_STEP_DEG = 72.0f;  // five exits are spaced 72 degrees apart
const int RB_TAKE = 4;
const float RB_ARC = RB_RADIUS * RB_EXIT_STEP_DEG * RB_TAKE * DEG_TO_RAD;
const float RB_EXIT_BEARING = 180.0f - RB_EXIT_STEP_DEG * RB_TAKE;  // traffic circulates counter-clockwise
const float RB_WINDOW = 12.0f;

float rbSmooth(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * (3.0f - 2.0f * t);
}

float rbBlend(float from, float to, float t) {
  const float delta = fmodf(to - from + 1260.0f, 360.0f) - 180.0f;
  return fmodf(from + delta * t + 720.0f, 360.0f);
}

// Compass angle, seen from the centre, of the point on the ring that distance s has reached.
float rbRingAngle(float s) {
  float along = s - RB_APPROACH;
  along = along < 0.0f ? 0.0f : (along > RB_ARC ? RB_ARC : along);
  return 180.0f - along / (RB_RADIUS * DEG_TO_RAD);
}

void rbLocate(float s, float& x, float& y, float& heading) {
  const float exitRad = RB_EXIT_BEARING * DEG_TO_RAD;
  if (s <= RB_APPROACH) {
    x = 0.0f;
    y = -RB_RADIUS - (RB_APPROACH - s);
  } else if (s <= RB_APPROACH + RB_ARC) {
    const float a = rbRingAngle(s) * DEG_TO_RAD;
    x = RB_RADIUS * sinf(a);
    y = RB_RADIUS * cosf(a);
  } else {
    const float out = RB_RADIUS + (s - RB_APPROACH - RB_ARC);
    x = out * sinf(exitRad);
    y = out * cosf(exitRad);
  }
  const float enter = RB_APPROACH;
  const float leave = RB_APPROACH + RB_ARC;
  const float ring = rbRingAngle(s) - 90.0f;
  if (s < enter - RB_WINDOW) {
    heading = 0.0f;
  } else if (s < enter + RB_WINDOW) {
    heading = rbBlend(0.0f, ring, rbSmooth((s - (enter - RB_WINDOW)) / (2.0f * RB_WINDOW)));
  } else if (s < leave - RB_WINDOW) {
    heading = fmodf(ring + 720.0f, 360.0f);
  } else if (s < leave + RB_WINDOW) {
    heading = rbBlend(ring, RB_EXIT_BEARING, rbSmooth((s - (leave - RB_WINDOW)) / (2.0f * RB_WINDOW)));
  } else {
    heading = fmodf(RB_EXIT_BEARING + 720.0f, 360.0f);
  }
}

float rbTargetSpeed(float s, float toNext) {
  if (s < RB_APPROACH) return fminf(10.0f, 4.5f + toNext * 0.12f);
  if (s < RB_APPROACH + RB_ARC) return 5.0f;
  return fminf(9.0f, 3.5f + toNext * 0.12f);
}

// World point to canvas pixels around the user; off-canvas points become the gap marker (-1, -1).
void rbCanvas(float wx, float wy, float ux, float uy, int16_t& cx, int16_t& cy) {
  const float px = 204.0f + (wx - ux) * RB_PX_PER_M;
  const float py = 204.0f - (wy - uy) * RB_PX_PER_M;
  if (px < 0.0f || px > 407.0f || py < 0.0f || py > 407.0f) {
    cx = -1;
    cy = -1;
  } else {
    cx = static_cast<int16_t>(lroundf(px));
    cy = static_cast<int16_t>(lroundf(py));
  }
}

void rbAddStreet(NavState& s, float ax, float ay, float bx, float by, float ux, float uy) {
  if (s.streetSegmentCount >= NAV_MAX_STREET_SEGMENTS) return;
  int16_t x0, y0, x1, y1;
  rbCanvas(ax, ay, ux, uy, x0, y0);
  rbCanvas(bx, by, ux, uy, x1, y1);
  if (x0 < 0 || x1 < 0) return;
  const int i = s.streetSegmentCount++;
  s.streetX0[i] = x0; s.streetY0[i] = y0; s.streetX1[i] = x1; s.streetY1[i] = y1;
}

void rbFillFrame(NavState& s, float distance, float toNext, float wx, float wy) {
  const float leave = RB_APPROACH + RB_ARC;
  s.distanceM = static_cast<int>(toNext);
  if (distance < RB_APPROACH) {
    s.maneuver = MAN_ROUNDABOUT;
    strncpy(s.instruction, "At the roundabout take the 4th exit", sizeof(s.instruction) - 1);
  } else if (distance < leave) {
    s.maneuver = MAN_ROUNDABOUT;
    strncpy(s.instruction, "Take the 4th exit", sizeof(s.instruction) - 1);
  } else {
    s.maneuver = MAN_ARRIVE;
    strncpy(s.instruction, "Destination ahead", sizeof(s.instruction) - 1);
  }
  strncpy(s.street, "Rotary Avenue", sizeof(s.street) - 1);
  s.remainingM = static_cast<int>(RB_TOTAL - distance);
  s.remainingS = static_cast<int>(s.remainingM / 7.0f);
  const int eta = 18 * 60 + 20 + s.remainingS / 60;
  snprintf(s.eta, sizeof(s.eta), "%02d:%02d", eta / 60 % 24, eta % 60);
  s.tripProgress = distance / RB_TOTAL;

  // The ring and its five arms; the first arm is the road the ride arrives on.
  for (int a = 0; a < 360; a += 12) {
    const float r0 = a * DEG_TO_RAD;
    const float r1 = (a + 12) * DEG_TO_RAD;
    rbAddStreet(s, RB_RADIUS * sinf(r0), RB_RADIUS * cosf(r0), RB_RADIUS * sinf(r1), RB_RADIUS * cosf(r1), wx, wy);
  }
  for (int arm = 0; arm < 5; arm++) {
    const float bearing = (180.0f - RB_EXIT_STEP_DEG * arm) * DEG_TO_RAD;
    for (int piece = 0; piece < 12; piece++) {
      const float d0 = RB_RADIUS + piece * 25.0f;
      const float d1 = d0 + 25.0f;
      rbAddStreet(s, d0 * sinf(bearing), d0 * cosf(bearing), d1 * sinf(bearing), d1 * cosf(bearing), wx, wy);
    }
  }

  // Route ahead, coarse on the straight parts and finer around the ring.
  float p = distance;
  while (s.routeCount < NAV_MAX_ROUTE_POINTS - 1) {
    float x, y, h;
    rbLocate(p, x, y, h);
    rbCanvas(x, y, wx, wy, s.routeX[s.routeCount], s.routeY[s.routeCount]);
    s.routeCount++;
    if (p >= RB_TOTAL) break;
    float next = p + ((p >= RB_APPROACH && p < leave) ? 10.0f : 16.0f);
    if (p < RB_APPROACH && next > RB_APPROACH) next = RB_APPROACH;
    if (p < leave && next > leave) next = leave;
    p = next > RB_TOTAL ? RB_TOTAL : next;
  }
  // Travelled part, oldest point first, ending at the user.
  float back[NAV_MAX_DONE_POINTS];
  int backCount = 0;
  for (float q = distance; q >= 0.0f && backCount < NAV_MAX_DONE_POINTS; q -= 10.0f) back[backCount++] = q;
  for (int i = backCount - 1; i >= 0; i--) {
    float x, y, h;
    rbLocate(back[i], x, y, h);
    rbCanvas(x, y, wx, wy, s.routeDoneX[s.routeDoneCount], s.routeDoneY[s.routeDoneCount]);
    s.routeDoneCount++;
  }

  float dx, dy, dh;
  rbLocate(RB_TOTAL, dx, dy, dh);
  rbCanvas(dx, dy, wx, wy, s.destX, s.destY);
  s.hasDest = s.destX >= 0;
}

}  // namespace

void benchTick() {
  if (!benchActive) {
    return;
  }
  float& distance = simDistance;
  float& speed = simSpeed;
  uint32_t& lastStepMs = simLastStepMs;
  uint32_t& lastFrameMs = simLastFrameMs;
  uint32_t& arrivedAtMs = simArrivedAtMs;

  const uint32_t now = millis();
  if (arrivedAtMs != 0) {
    if (now - arrivedAtMs > 5000) {  // ride again
      arrivedAtMs = 0;
      distance = 0.0f;
      speed = 6.0f;
      hasNav = false;
      showingArrived = false;
      lastStepMs = 0;
    }
    return;
  }

  const float total = simRoundabout ? RB_TOTAL : simDistanceTo(SIM_POINTS - 1);
  int nextCorner = 1;
  float toNext;
  if (simRoundabout) {
    const float leaveRing = RB_APPROACH + RB_ARC;
    toNext = distance < RB_APPROACH ? RB_APPROACH - distance : (distance < leaveRing ? leaveRing - distance : total - distance);
  } else {
    while (nextCorner < SIM_POINTS - 1 && simDistanceTo(nextCorner) <= distance) nextCorner++;
    toNext = simDistanceTo(nextCorner) - distance;
  }

  const float dt = lastStepMs == 0 ? 0.0f : (now - lastStepMs) * 0.001f;
  lastStepMs = now;
  const float target = simRoundabout ? rbTargetSpeed(distance, toNext) : fminf(13.0f, 4.5f + toNext * 0.12f);  // slows down for each turn
  speed += (target - speed) * fminf(1.0f, dt * 1.2f);
  distance += speed * dt;

  if (total - distance < 12.0f) {
    arrivedAtMs = now;
    hasNav = false;
    awaitingFirstNav = false;
    showingArrived = true;
    motion.reset();
    showArrivalScreen();
    return;
  }
  if (hasNav && now - lastFrameMs < 1000) {
    // Between the GPS-rate full frames the phone sends a motion update every 100 ms: the newest
    // (gyro-fused) heading and the last fix with its age.
    if (now - simLastMotionMs >= 100) {
      simLastMotionMs = now;
      int motionLeg;
      float mx, my, motionHeading;
      if (simRoundabout) {
        rbLocate(distance, mx, my, motionHeading);
      } else {
        simLocate(distance, motionLeg, mx, my, motionHeading);
      }
      MotionUpdate update;
      update.lat = simFixLat;
      update.lon = simFixLon;
      update.bearing = motionHeading;
      update.speedMps = speed;
      update.bearingValid = true;
      update.ageMs = static_cast<uint16_t>(now - lastFrameMs);
      applyMotionUpdate(update);
    }
    return;
  }
  lastFrameMs = now;

  int leg;
  float wx, wy, heading;
  if (simRoundabout) {
    rbLocate(distance, wx, wy, heading);
  } else {
    simLocate(distance, leg, wx, wy, heading);
  }

  NavState s;
  s.valid = true;
  s.style = benchMinimal ? STYLE_MINIMAL : STYLE_CLASSIC;
  s.headingUpMode = true;
  s.bearingValid = true;
  s.bearing = heading;
  s.speedMps = speed;
  s.pxPerM = simRoundabout ? RB_PX_PER_M : SIM_PX_PER_M;
  s.lat = wy / METERS_PER_DEG;
  s.lon = wx / METERS_PER_DEG;
  simFixLat = s.lat;
  simFixLon = s.lon;
  s.userX = 204;
  s.userY = 204;
  s.hasUserPosition = true;

  if (simRoundabout) {
    rbFillFrame(s, distance, toNext, wx, wy);
    applyNavFrame(s);
    Serial.printf("demo: %4.0f m  heading %3.0f  %4.1f m/s  next %s in %d m  zoom %.2f\n", distance, heading, speed,
                  s.instruction, s.distanceM, zoom.zoom());
    return;
  }

  s.distanceM = static_cast<int>(toNext);
  s.maneuver = SIM_MANEUVER[nextCorner];
  strncpy(s.instruction, SIM_INSTRUCTION[nextCorner], sizeof(s.instruction) - 1);
  strncpy(s.street, SIM_STREET[nextCorner < 3 ? nextCorner : 3], sizeof(s.street) - 1);
  s.remainingM = static_cast<int>(total - distance);
  s.remainingS = static_cast<int>(s.remainingM / 9.0f);
  const int eta = 18 * 60 + 20 + s.remainingS / 60;
  snprintf(s.eta, sizeof(s.eta), "%02d:%02d", eta / 60 % 24, eta % 60);
  s.tripProgress = distance / total;

  // City blocks centred on the user like the phone does; the route runs along them.
  const float half = 204.0f / SIM_PX_PER_M;
  for (double gx = floor((wx - half) / SIM_GRID_M) * SIM_GRID_M; gx < wx + half; gx += SIM_GRID_M) {
    if (s.streetSegmentCount >= NAV_MAX_STREET_SEGMENTS) break;
    const int x = static_cast<int>(204 + (gx - wx) * SIM_PX_PER_M);
    const int i = s.streetSegmentCount++;
    s.streetX0[i] = x; s.streetY0[i] = 4; s.streetX1[i] = x; s.streetY1[i] = 404;
  }
  for (double gy = floor((wy - half) / SIM_GRID_M) * SIM_GRID_M; gy < wy + half; gy += SIM_GRID_M) {
    if (s.streetSegmentCount >= NAV_MAX_STREET_SEGMENTS) break;
    const int y = static_cast<int>(204 - (gy - wy) * SIM_PX_PER_M);
    const int i = s.streetSegmentCount++;
    s.streetX0[i] = 4; s.streetY0[i] = y; s.streetX1[i] = 404; s.streetY1[i] = y;
  }

  simAddRoutePoint(s, wx, wy, wx, wy);
  for (int corner = nextCorner; corner < SIM_POINTS; corner++) {
    simAddRoutePoint(s, SIM_ROUTE[corner].x, SIM_ROUTE[corner].y, wx, wy);
  }
  // Travelled part: the start and the corners already passed, ending at the user.
  for (int corner = 0; corner < nextCorner && s.routeDoneCount < NAV_MAX_DONE_POINTS - 1; corner++) {
    s.routeDoneX[s.routeDoneCount] = static_cast<int16_t>(204 + (SIM_ROUTE[corner].x - wx) * SIM_PX_PER_M);
    s.routeDoneY[s.routeDoneCount] = static_cast<int16_t>(204 - (SIM_ROUTE[corner].y - wy) * SIM_PX_PER_M);
    s.routeDoneCount++;
  }
  s.routeDoneX[s.routeDoneCount] = 204;
  s.routeDoneY[s.routeDoneCount] = 204;
  s.routeDoneCount++;

  const int destX = static_cast<int>(204 + (SIM_ROUTE[SIM_POINTS - 1].x - wx) * SIM_PX_PER_M);
  const int destY = static_cast<int>(204 - (SIM_ROUTE[SIM_POINTS - 1].y - wy) * SIM_PX_PER_M);
  if (destX >= 0 && destX < 408 && destY >= 0 && destY < 408) {
    s.hasDest = true;
    s.destX = static_cast<int16_t>(destX);
    s.destY = static_cast<int16_t>(destY);
  }
  applyNavFrame(s);
  Serial.printf("demo: %4.0f m  heading %3.0f  %4.1f m/s  next %s in %d m  zoom %.2f\n", distance, heading, speed,
                SIM_INSTRUCTION[nextCorner], s.distanceM, zoom.zoom());
}
