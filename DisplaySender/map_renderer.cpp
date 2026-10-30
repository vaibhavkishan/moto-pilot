#include "map_renderer.h"
#include "html_renderer.h"
#include "maps_theme.h"
#include "scene.h"
#include "utf8_text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.01745329251f
#endif

static const uint8_t STREET_EDGE_W = 4;
static const uint8_t STREET_FILL_W = 2;
static const uint8_t ROUTE_EDGE_W = 6;
static const uint8_t ROUTE_FILL_W = 3;

#define SW_W 48
#define SW_H 24
#define SW_X (SCR_W - 8 - SW_W)
#define SW_Y (MAP_AREA_H + 4)

/* Bottom sheet: maneuver icon on the left, text column to its right, trip summary underneath. */
#define ICON_X 8
#define ICON_Y (MAP_AREA_H + 4)
#define TEXT_X 64
#define DIST_Y (MAP_AREA_H + 4)
#define DIST_W 136
#define DIST_H 26
#define INSTR_Y (MAP_AREA_H + 31)
#define STREET_Y (MAP_AREA_H + 51)
#define INFO_Y (MAP_AREA_H + 71)

/* Colours the map layer uses. The 4-bit sprite stores these indices; the palette maps them to RGB565. */
enum PaletteIndex : uint8_t {
  PAL_LAND,
  PAL_PARK,
  PAL_ROAD_EDGE,
  PAL_ROAD_FILL,
  PAL_ROUTE_EDGE,
  PAL_ROUTE,
  PAL_SHADOW,
  PAL_OUTLINE,
  PAL_ARROW,
  PAL_ARROW_EDGE,
  PAL_DONE_EDGE,
  PAL_DONE,
  PAL_DEST,
  PAL_DEST_EDGE,
  PAL_COUNT
};

static const uint16_t kIdentityPalette[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

static void fillThemePalette(uint16_t* p) {
  memset(p, 0, 16 * sizeof(uint16_t));
  p[PAL_LAND] = mapsColLand();
  p[PAL_PARK] = mapsColPark();
  p[PAL_ROAD_EDGE] = mapsColRoadEdge();
  p[PAL_ROAD_FILL] = mapsColRoadFill();
  p[PAL_ROUTE_EDGE] = mapsColRouteEdge();
  p[PAL_ROUTE] = mapsColRoute();
  p[PAL_SHADOW] = mapsColCardShadow();
  p[PAL_OUTLINE] = mapsColArrowOutline();
  p[PAL_ARROW] = mapsColArrowFill();
  p[PAL_ARROW_EDGE] = mapsColArrowEdge();
  p[PAL_DONE_EDGE] = mapsColRouteDoneEdge();
  p[PAL_DONE] = mapsColRouteDone();
  p[PAL_DEST] = mapsColDestination();
  p[PAL_DEST_EDGE] = mapsColDestinationEdge();
}

/* Cheap reject for segments that are entirely off the same side of the map area. */
static inline bool segmentOffscreen(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  const int m = 8;
  return (x0 < -m && x1 < -m) || (x0 > SCR_W + m && x1 > SCR_W + m) ||
         (y0 < -m && y1 < -m) || (y0 > MAP_AREA_H + m && y1 > MAP_AREA_H + m);
}

/* Uniform-width line with round ends: two triangles for the body, discs at the joints. */
static void drawThickLine(TFT_eSPI& g, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          uint16_t color, uint8_t width) {
  if (width <= 1) {
    g.drawLine(x0, y0, x1, y1, color);
    return;
  }
  const int radius = width / 2;
  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  const float length = sqrtf(dx * dx + dy * dy);
  if (length < 0.5f) {
    g.fillCircle(x0, y0, radius, color);
    return;
  }
  const float half = static_cast<float>(width) * 0.5f;
  const float nx = -dy / length * half;
  const float ny = dx / length * half;
  const int16_t ax = static_cast<int16_t>(lroundf(x0 + nx)), ay = static_cast<int16_t>(lroundf(y0 + ny));
  const int16_t bx = static_cast<int16_t>(lroundf(x0 - nx)), by = static_cast<int16_t>(lroundf(y0 - ny));
  const int16_t cx = static_cast<int16_t>(lroundf(x1 - nx)), cy = static_cast<int16_t>(lroundf(y1 - ny));
  const int16_t ex = static_cast<int16_t>(lroundf(x1 + nx)), ey = static_cast<int16_t>(lroundf(y1 + ny));
  g.fillTriangle(ax, ay, bx, by, cx, cy, color);
  g.fillTriangle(ax, ay, cx, cy, ex, ey, color);
  if (width >= 3) {
    g.fillCircle(x0, y0, radius, color);
    g.fillCircle(x1, y1, radius, color);
  }
}

/* Chevron about 12 px tall at scale 0.5, centred on (cx, cy); a larger scale gives the white edge. */
static void fillArrow(TFT_eSPI& g, int cx, int cy, float deg, float scale, uint16_t color) {
  static const float P[4][2] = {{0.0f, -12.0f}, {-10.0f, 12.0f}, {0.0f, 6.0f}, {10.0f, 12.0f}};
  const float rad = deg * DEG_TO_RAD;
  const float s = sinf(rad);
  const float c = cosf(rad);
  int16_t x[4];
  int16_t y[4];
  for (int i = 0; i < 4; i++) {
    const float lx = P[i][0] * scale;
    const float ly = P[i][1] * scale;
    x[i] = static_cast<int16_t>(cx + lroundf(lx * c - ly * s));
    y[i] = static_cast<int16_t>(cy + lroundf(lx * s + ly * c));
  }
  g.fillTriangle(x[0], y[0], x[1], y[1], x[2], y[2], color);
  g.fillTriangle(x[0], y[0], x[3], y[3], x[2], y[2], color);
}

static void drawMarker(TFT_eSPI& g, const uint16_t* pal, int cx, int cy, const NavState& state,
                       const MapView& view) {
  const bool dark = mapsThemeIsDark();
  if (!view.headingKnown) {
    if (!dark) {
      g.fillCircle(cx, cy, 7, pal[PAL_ARROW_EDGE]);
    }
    g.fillCircle(cx, cy, 6, pal[PAL_OUTLINE]);
    g.fillCircle(cx, cy, 4, pal[PAL_ARROW]);
    return;
  }
  /* In heading-up mode the map is already rotated, so the arrow always points up. */
  const float deg = state.headingUpMode ? 0.0f : view.headingDeg;
  if (!dark) {
    /* Dark outer edge, white ring, blue fill: readable on white roads and on the blue route. */
    fillArrow(g, cx, cy, deg, 0.78f, pal[PAL_ARROW_EDGE]);
  }
  fillArrow(g, cx, cy, deg, 0.66f, pal[PAL_OUTLINE]);
  fillArrow(g, cx, cy, deg, 0.5f, pal[PAL_ARROW]);
}

/* Red map pin whose tip sits on (x, y). */
static void drawDestination(TFT_eSPI& g, const uint16_t* pal, int x, int y) {
  drawPin(g, x, y, 1.0f, pal[PAL_DEST], pal[PAL_DEST_EDGE]);
}

/* Transforms a source polyline to the screen and draws its casing, then its fill. */
static void drawPolyline(TFT_eSPI& g, const MapXf& xf, const int16_t* srcX, const int16_t* srcY, int count,
                         uint16_t edgeColor, uint16_t fillColor) {
  if (count < 2) {
    return;
  }
  static int16_t px[NAV_MAX_ROUTE_POINTS];
  static int16_t py[NAV_MAX_ROUTE_POINTS];
  static bool ok[NAV_MAX_ROUTE_POINTS];
  for (int i = 0; i < count; i++) {
    /* Negative source coordinates mark a gap in the polyline. */
    ok[i] = srcX[i] >= 0 && srcY[i] >= 0;
    if (ok[i]) {
      xfApply(xf, srcX[i], srcY[i], px[i], py[i]);
    }
  }
  for (int pass = 0; pass < 2; pass++) {
    const uint16_t color = pass == 0 ? edgeColor : fillColor;
    const uint8_t width = (pass == 0) ? ROUTE_EDGE_W : ROUTE_FILL_W;
    for (int i = 0; i < count - 1; i++) {
      if (!ok[i] || !ok[i + 1] || segmentOffscreen(px[i], py[i], px[i + 1], py[i + 1])) {
        continue;
      }
      drawThickLine(g, px[i], py[i], px[i + 1], py[i + 1], color, width);
    }
  }
}

/* Arrow head whose base centre is (ex, ey), pointing along the unit vector (dx, dy). */
static void iconArrowHead(TFT_eSPI& g, int ox, int oy, float ex, float ey, float dx, float dy,
                          uint16_t color) {
  const float length = 11.0f;
  const float half = 9.0f;
  g.fillTriangle(ox + lroundf(ex + dx * length), oy + lroundf(ey + dy * length),
                 ox + lroundf(ex - dy * half), oy + lroundf(ey + dx * half),
                 ox + lroundf(ex + dy * half), oy + lroundf(ey - dx * half), color);
}

/* Thick polyline with an arrow head on the last point, in a 48x48 icon box at (ox, oy). */
static void iconPath(TFT_eSPI& g, int ox, int oy, const int8_t (*p)[2], int count, uint16_t color) {
  for (int i = 0; i < count - 1; i++) {
    drawThickLine(g, ox + p[i][0], oy + p[i][1], ox + p[i + 1][0], oy + p[i + 1][1], color, 6);
  }
  const float dx = static_cast<float>(p[count - 1][0] - p[count - 2][0]);
  const float dy = static_cast<float>(p[count - 1][1] - p[count - 2][1]);
  const float length = sqrtf(dx * dx + dy * dy);
  iconArrowHead(g, ox, oy, p[count - 1][0], p[count - 1][1], dx / length, dy / length, color);
}

#define ICON_PATH(...)                                              \
  do {                                                              \
    static const int8_t path[][2] = {__VA_ARGS__};                  \
    iconPath(g, ox, oy, path, sizeof(path) / sizeof(path[0]), fg);  \
  } while (0)

static void drawManeuverShape(TFT_eSPI& g, int ox, int oy, uint8_t maneuver, uint16_t fg, uint16_t bg) {
  switch (maneuver) {
    case MAN_RIGHT:        ICON_PATH({12, 46}, {12, 28}, {28, 28}); break;
    case MAN_LEFT:         ICON_PATH({36, 46}, {36, 28}, {20, 28}); break;
    case MAN_SLIGHT_RIGHT: ICON_PATH({14, 46}, {14, 32}, {28, 18}); break;
    case MAN_SLIGHT_LEFT:  ICON_PATH({34, 46}, {34, 32}, {20, 18}); break;
    case MAN_SHARP_RIGHT:  ICON_PATH({10, 46}, {10, 20}, {30, 36}); break;
    case MAN_SHARP_LEFT:   ICON_PATH({38, 46}, {38, 20}, {18, 36}); break;
    case MAN_UTURN:        ICON_PATH({34, 46}, {34, 22}, {29, 13}, {19, 13}, {14, 22}, {14, 32}); break;
    case MAN_ROUNDABOUT:
      g.fillCircle(ox + 24, oy + 30, 15, fg);
      g.fillCircle(ox + 24, oy + 30, 9, bg);
      ICON_PATH({24, 20}, {24, 12});
      break;
    case MAN_ARRIVE:
      g.fillCircle(ox + 24, oy + 18, 13, fg);
      g.fillTriangle(ox + 12, oy + 25, ox + 36, oy + 25, ox + 24, oy + 45, fg);
      g.fillCircle(ox + 24, oy + 18, 5, bg);
      break;
    case MAN_DEPART:
      g.fillCircle(ox + 24, oy + 42, 5, fg);
      ICON_PATH({24, 38}, {24, 18});
      break;
    case MAN_NONE:
      break;
    default:  // MAN_STRAIGHT
      ICON_PATH({24, 46}, {24, 18});
      break;
  }
}

static void renderMap(TFT_eSPI& g, const uint16_t* pal, const NavState& state, const MapXf& xf,
                      const MapView& view) {
  g.fillRect(0, 0, SCR_W, MAP_AREA_H, pal[PAL_LAND]);
  if (!mapsThemeIsDark()) {
    g.fillCircle(SCR_W - 36, 70, 32, pal[PAL_PARK]);
    g.fillCircle(28, MAP_AREA_H - 60, 26, pal[PAL_PARK]);
  }

  static int16_t segX0[NAV_MAX_STREET_SEGMENTS];
  static int16_t segY0[NAV_MAX_STREET_SEGMENTS];
  static int16_t segX1[NAV_MAX_STREET_SEGMENTS];
  static int16_t segY1[NAV_MAX_STREET_SEGMENTS];
  int segCount = 0;
  for (int i = 0; i < state.streetSegmentCount; i++) {
    int16_t x0, y0, x1, y1;
    xfApply(xf, state.streetX0[i], state.streetY0[i], x0, y0);
    xfApply(xf, state.streetX1[i], state.streetY1[i], x1, y1);
    if (segmentOffscreen(x0, y0, x1, y1)) {
      continue;
    }
    segX0[segCount] = x0;
    segY0[segCount] = y0;
    segX1[segCount] = x1;
    segY1[segCount] = y1;
    segCount++;
  }
  for (int i = 0; i < segCount; i++) {
    drawThickLine(g, segX0[i], segY0[i], segX1[i], segY1[i], pal[PAL_ROAD_EDGE], STREET_EDGE_W);
  }
  for (int i = 0; i < segCount; i++) {
    drawThickLine(g, segX0[i], segY0[i], segX1[i], segY1[i], pal[PAL_ROAD_FILL], STREET_FILL_W);
  }

  drawPolyline(g, xf, state.routeDoneX, state.routeDoneY, state.routeDoneCount, pal[PAL_DONE_EDGE], pal[PAL_DONE]);
  drawPolyline(g, xf, state.routeX, state.routeY, state.routeCount, pal[PAL_ROUTE_EDGE], pal[PAL_ROUTE]);

  if (state.hasDest && state.destX >= 0 && state.destY >= 0) {
    int16_t dx, dy;
    xfApply(xf, state.destX, state.destY, dx, dy);
    if (dx > -10 && dx < SCR_W + 10 && dy > 0 && dy < MAP_AREA_H + 24) {
      drawDestination(g, pal, dx, dy);
    }
  }

  if (state.hasUserPosition && xf.ax >= 0 && xf.ay >= 0 && xf.ax < SCR_W && xf.ay < MAP_AREA_H) {
    drawMarker(g, pal, xf.ax, xf.ay, state, view);
  }

  g.fillRect(0, MAP_AREA_H - 3, SCR_W, 3, pal[PAL_SHADOW]);
}

MapRenderer::MapRenderer(TFT_eSPI& display)
    : tft(display), mapSpr_(&display), lineSpr_(&display), iconSpr_(&display) {}

bool MapRenderer::begin() {
  /* Largest block first so the big sprite gets contiguous heap. */
  mapSpr_.setColorDepth(4);
  mapSprOk_ = mapSpr_.createSprite(SCR_W, MAP_AREA_H) != nullptr;
  lineSpr_.setColorDepth(16);
  lineSprOk_ = lineSpr_.createSprite(LINE_W, LINE_H) != nullptr;
  iconSpr_.setColorDepth(16);
  iconSprOk_ = iconSpr_.createSprite(ICON_SIZE, ICON_SIZE) != nullptr;
  Serial.printf("Renderer: map sprite %s, line sprite %s, icon sprite %s, free heap %u, max block %u\n",
                mapSprOk_ ? "ok" : "FAILED (direct drawing)", lineSprOk_ ? "ok" : "FAILED",
                iconSprOk_ ? "ok" : "FAILED", static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
  return mapSprOk_;
}

void MapRenderer::invalidate() {
  overlayValid_ = false;
}

void MapRenderer::drawThemeSwitchAt(int sx, int sy) {
  drawThemeButton(tft, mapsThemeIsDark(), sx + SW_W - SW_H / 2, sy + SW_H / 2, SW_H / 2);
}

void MapRenderer::drawThemeSwitch() {
  drawThemeSwitchAt(SW_X, SW_Y);
}

void MapRenderer::drawThemeSwitchWaiting() {
  drawThemeButton(tft, mapsThemeIsDark(), 216, 300, 15);
}

bool MapRenderer::themeSwitchHit(uint16_t x, uint16_t y) {
  /* Generous hit zones — resistive touch on CYD is imprecise. */

  /* Nav overlay: right side of bottom sheet */
  if (y >= static_cast<uint16_t>(MAP_AREA_H) &&
      x >= static_cast<uint16_t>(SCR_W - 90)) {
    return true;
  }
  /* Waiting / status: bottom-right corner */
  if (y >= static_cast<uint16_t>(SCR_H - 70) &&
      x >= static_cast<uint16_t>(SCR_W - 90)) {
    return true;
  }
  return false;
}

void MapRenderer::draw(const NavState& state, const MapView& view) {
  const uint32_t started = micros();
  drawOverlay(state);
  drawMapLayer(state, view);
  lastDrawUs_ = micros() - started;
}

void MapRenderer::drawMapLayer(const NavState& state, const MapView& view) {
  const MapXf xf = makeXf(state, view);
  uint16_t themePalette[16];
  fillThemePalette(themePalette);

  if (mapSprOk_) {
    const bool dark = mapsThemeIsDark();
    if (!paletteValid_ || paletteDark_ != dark) {
      mapSpr_.createPalette(themePalette, PAL_COUNT);
      paletteValid_ = true;
      paletteDark_ = dark;
    }
    renderMap(mapSpr_, kIdentityPalette, state, xf, view);
    mapSpr_.pushSprite(0, 0);
  } else {
    renderMap(tft, themePalette, state, xf, view);
  }
}

void MapRenderer::drawOverlayBase() {
  tft.fillRect(0, MAP_AREA_H, SCR_W, OVERLAY_H, mapsColCard());
  tft.fillRoundRect(SCR_W / 2 - 14, MAP_AREA_H + 1, 28, 3, 1, mapsColMuted());
  drawThemeSwitchAt(SW_X, SW_Y);
}

void MapRenderer::drawManeuverIcon(uint8_t maneuver) {
  const uint16_t background = mapsColCard();
  const uint16_t foreground = mapsColText();
  if (iconSprOk_) {
    iconSpr_.fillSprite(background);
    drawManeuverShape(iconSpr_, 0, 0, maneuver, foreground, background);
    iconSpr_.pushSprite(ICON_X, ICON_Y);
  } else {
    tft.fillRect(ICON_X, ICON_Y, ICON_SIZE, ICON_SIZE, background);
    drawManeuverShape(tft, ICON_X, ICON_Y, maneuver, foreground, background);
  }
  iconShown_ = maneuver;
  iconValid_ = true;
}

void MapRenderer::drawTextLine(int index, int x, int y, int width, int height, const char* text,
                               uint16_t color, LineKind kind) {
  if (strcmp(lineCache_[index], text) == 0 && (text[0] == '\0' || lineColor_[index] == color)) {
    return;
  }
  strncpy(lineCache_[index], text, sizeof(lineCache_[index]) - 1);
  lineCache_[index][sizeof(lineCache_[index]) - 1] = '\0';
  lineColor_[index] = color;

  const uint16_t background = mapsColCard();
  TFT_eSPI* g = &tft;
  int left = x;
  int top = y;
  if (lineSprOk_) {
    lineSpr_.fillSprite(background);
    g = &lineSpr_;
    left = 0;
    top = 0;
  } else {
    tft.fillRect(x, y, width, height, background);
  }
  if (text[0] != '\0') {
    if (kind == LINE_UTF8) {
      drawUtf8Text(*g, text, left, top, width, color, background);
    } else {
      g->setTextColor(color, background);
      g->setTextDatum(TL_DATUM);
      g->drawString(text, left, top, kind == LINE_FONT4 ? 4 : 2);
    }
  }
  if (lineSprOk_) {
    lineSpr_.pushSprite(x, y, 0, 0, width, height);
  }
}

void MapRenderer::drawOverlay(const NavState& state) {
  const bool dark = mapsThemeIsDark();
  const bool html = state.hasHtml && state.html[0] != '\0';
  bool repaint = !overlayValid_ || dark != overlayDark_ || html != overlayHtml_;
  if (html && strcmp(htmlCache_, state.html) != 0) {
    repaint = true;
  }

  if (repaint) {
    drawOverlayBase();
    overlayValid_ = true;
    overlayDark_ = dark;
    overlayHtml_ = html;
    iconValid_ = false;
    memset(lineCache_, 0, sizeof(lineCache_));
    memset(lineColor_, 0, sizeof(lineColor_));
    htmlCache_[0] = '\0';
    if (html) {
      strncpy(htmlCache_, state.html, sizeof(htmlCache_) - 1);
      htmlCache_[sizeof(htmlCache_) - 1] = '\0';
      HtmlRenderer htmlRenderer(tft);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(mapsColAccent(), mapsColCard());
      htmlRenderer.drawLines(state.html, 8, MAP_AREA_H + 16, 20, 3);
      drawThemeSwitchAt(SW_X, SW_Y);
    }
  }
  if (html) {
    return;
  }

  char distLine[24];
  if (state.distanceM >= 1000) {
    snprintf(distLine, sizeof(distLine), "%.1f km", state.distanceM / 1000.0f);
  } else {
    snprintf(distLine, sizeof(distLine), "%d m", state.distanceM);
  }
  const bool lost = state.rerouting || state.offRoute;
  const char* distOrGps = state.gpsWeak ? "Weak GPS" : lost ? (state.rerouting ? "Rerouting" : "Off route") : distLine;
  const uint16_t distColor = (lost && !state.gpsWeak) ? mapsColWarning() : mapsColAccent();

  char remaining[48] = "";
  uint16_t remainingColor = mapsColMuted();
  if (lost) {
    /* The large line already says so, and the distance to the next turn is meaningless here. */
  } else if (state.remainingM >= 0 || state.remainingS >= 0) {
    char km[16] = "--";
    char mins[16] = "--";
    if (state.remainingM >= 0) {
      snprintf(km, sizeof(km), state.remainingM < 1000 ? "%.2f km" : "%.1f km", state.remainingM / 1000.0);
    }
    if (state.remainingS >= 0) {
      const int minutes = state.remainingS / 60 + (state.remainingS % 60 != 0);
      if (minutes >= 60) {
        snprintf(mins, sizeof(mins), "%dh%02d", minutes / 60, minutes % 60);
      } else {
        snprintf(mins, sizeof(mins), "%d min", minutes);
      }
    }
    snprintf(remaining, sizeof(remaining), "%s | %s", km, mins);
    const size_t used = strlen(remaining);
    if (state.eta[0] != '\0' && used < sizeof(remaining) - 1) {
      snprintf(remaining + used, sizeof(remaining) - used, " | ETA %s", state.eta);
    }
  }

  if (!iconValid_ || iconShown_ != state.maneuver) {
    drawManeuverIcon(state.maneuver);
  }
  const int textW = SCR_W - TEXT_X - 8;
  drawTextLine(0, TEXT_X, DIST_Y, DIST_W, DIST_H, distOrGps, distColor, LINE_FONT4);
  drawTextLine(1, TEXT_X, INSTR_Y, textW, 20, state.instruction, mapsColText(), LINE_UTF8);
  drawTextLine(2, TEXT_X, STREET_Y, textW, 19, state.street, mapsColMuted(), LINE_UTF8);
  drawTextLine(3, 8, INFO_Y, SCR_W - 16, 16, remaining, remainingColor, LINE_FONT2);
}
