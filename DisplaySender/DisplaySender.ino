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

static NavState lastNav;
static bool hasNav = false;

// Rendering is decoupled from BLE parsing: frames only update lastNav, loop() draws the latest.
static HeadingFilter headingFilter;
static bool navDirty = false;
static uint32_t lastRenderMs = 0;
static const uint32_t RENDER_INTERVAL_MS = 50;
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
void renderTick();
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
  renderTick();

  if (clientConnected && awaitingFirstNav) {
    updateMapLoadingAnimation(tft, millis());
  }

  updateStatsCounter();
  delay(5);
}

void handleUiFlags() {
  if (uiShowLoading) {
    uiShowLoading = false;
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
  if (hasNav && clientConnected) {
    mapRenderer.invalidate();
    navDirty = true;
    showingWaiting = false;
  } else if (clientConnected && showingArrived) {
    drawArrivalScreen(tft, mapsThemeIsDark());
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
  uint16_t x = 0;
  uint16_t y = 0;
  const bool down = touchRead(x, y);
  const uint32_t now = millis();

  if (down && !touchWasDown) {
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
    showMapLoadingScreen(tft);
    return;
  }

  if (isArrivedJson(payload, length)) {
    awaitingFirstNav = false;
    hasNav = false;
    showingWaiting = false;
    showingArrived = true;
    drawArrivalScreen(tft, mapsThemeIsDark());
    return;
  }

  NavState state;
  if (!parseNavJson(payload, length, state)) {
    return;
  }

  if (!hasNav) {
    // First frame after connect/loading: the screen holds something else and the heading is stale.
    headingFilter.reset();
    mapRenderer.invalidate();
  }
  // Frames without bearing_valid give no way to tell a real course apart.
  headingFilter.onSample(state.bearing, state.headingUpMode ? state.bearingValid : true);

  lastNav = state;
  hasNav = true;
  showingWaiting = false;
  showingArrived = false;
  navDirty = true;
  awaitingFirstNav = false;
  navUpdatesReceived++;
  updatesPerSecond++;
}

void renderTick() {
  if (!hasNav || !clientConnected) {
    return;
  }
  const uint32_t now = millis();
  if (now - lastRenderMs < RENDER_INTERVAL_MS) {
    return;
  }
  const bool headingMoved = headingFilter.tick(now);
  if (!navDirty && !headingMoved) {
    return;
  }
  navDirty = false;
  lastRenderMs = now;

  MapView view;
  view.headingKnown = headingFilter.known();
  view.headingDeg = headingFilter.display();
  mapRenderer.draw(lastNav, view);
  headingFilter.markDrawn();

  const uint32_t us = mapRenderer.lastDrawUs();
  rendersPerSecond++;
  renderUsSum += us;
  if (us > renderUsMax) renderUsMax = us;
}

void updateStatsCounter() {
  const uint32_t now = millis();
  if (now - lastStatsMillis >= 1000) {
    if (clientConnected && navUpdatesReceived > 0) {
      Serial.printf("Nav updates/s: %u  Total: %lu  Renders/s: %u  draw avg/max: %lu/%lu us  heap: %u (min %u)  sprite: %s\n",
                    updatesPerSecond,
                    static_cast<unsigned long>(navUpdatesReceived),
                    rendersPerSecond,
                    static_cast<unsigned long>(rendersPerSecond ? renderUsSum / rendersPerSecond : 0),
                    static_cast<unsigned long>(renderUsMax),
                    static_cast<unsigned>(ESP.getFreeHeap()),
                    static_cast<unsigned>(ESP.getMinFreeHeap()),
                    mapRenderer.usesSprite() ? "yes" : "no");
    }
    updatesPerSecond = 0;
    rendersPerSecond = 0;
    renderUsSum = 0;
    renderUsMax = 0;
    lastStatsMillis = now;
  }
}
