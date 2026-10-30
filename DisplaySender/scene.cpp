#include "scene.h"

#include "logo.h"
#include "maps_theme.h"
#include "nav_types.h"

#include <math.h>

namespace {

struct Rgb {
  uint8_t r, g, b;
};

uint16_t c565(Rgb c) {
  return static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

Rgb mix(Rgb a, Rgb b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return {static_cast<uint8_t>(a.r + (b.r - a.r) * t + 0.5f),
          static_cast<uint8_t>(a.g + (b.g - a.g) * t + 0.5f),
          static_cast<uint8_t>(a.b + (b.b - a.b) * t + 0.5f)};
}

struct Palette {
  Rgb skyTop, skyBottom, sun, cloud, snow;
  Rgb mtn[4], mtnLit[4];
  Rgb ridge, lake, treeFar, treeNear, treeRight;
  Rgb vergeL0, vergeL1, vergeR0, vergeR1, rock, rockDark;
  Rgb road0, road1, edge, dash, glowHalo, glow, glowCore;
  Rgb title, subtitle, panel, track, bar, text2;
  Rgb card, cardBorder, cardLabel, btn, btnIcon;
};

const Palette kDay = {
    {150, 200, 250}, {226, 240, 252}, {255, 247, 215}, {244, 249, 255}, {236, 245, 255},
    {{120, 165, 228}, {138, 182, 238}, {152, 196, 244}, {112, 156, 220}},
    {{170, 205, 248}, {182, 214, 250}, {196, 224, 252}, {160, 198, 244}},
    {92, 132, 196}, {160, 200, 240}, {70, 108, 170}, {34, 60, 112}, {46, 84, 140},
    {86, 122, 184}, {44, 68, 128}, {92, 128, 186}, {40, 64, 118}, {96, 132, 192}, {50, 76, 136},
    {52, 64, 92}, {14, 24, 44}, {240, 246, 255}, {225, 232, 245}, {60, 120, 240}, {30, 110, 255}, {200, 225, 255},
    {16, 24, 48}, {96, 108, 128}, {230, 239, 252}, {176, 194, 228}, {26, 115, 232}, {70, 100, 160},
    {24, 38, 66}, {64, 84, 120}, {125, 150, 196}, {206, 225, 255}, {30, 70, 140}};

const Palette kNight = {
    {8, 14, 38}, {40, 66, 124}, {232, 238, 252}, {56, 78, 132}, {110, 140, 200},
    {{28, 48, 98}, {34, 56, 110}, {40, 64, 122}, {26, 44, 90}},
    {{44, 70, 130}, {50, 80, 142}, {58, 90, 154}, {42, 68, 126}},
    {24, 42, 86}, {44, 72, 130}, {22, 40, 84}, {8, 18, 44}, {12, 26, 58},
    {30, 50, 98}, {14, 24, 54}, {32, 52, 100}, {12, 22, 50}, {38, 60, 112}, {18, 30, 66},
    {30, 38, 60}, {8, 12, 24}, {200, 212, 236}, {190, 200, 225}, {40, 90, 220}, {50, 130, 255}, {190, 220, 255},
    {255, 255, 255}, {170, 184, 210}, {16, 24, 44}, {40, 54, 88}, {102, 160, 246}, {150, 170, 205},
    {14, 22, 42}, {52, 70, 108}, {120, 146, 196}, {56, 76, 122}, {220, 230, 255}};

constexpr int HORIZON = 176;
constexpr int LOAD_ART_H = 236;

float roadHalfWidth(float y) {
  if (y < HORIZON) return 0.0f;
  const float d = y - HORIZON;
  return d < 7.0f ? 2.0f + d * 0.3f : 4.1f + 2.3f * (d - 7.0f);
}

float roadCenter(float y) {
  const float d = y - HORIZON;
  return 124.0f + 34.0f * sinf(d / 24.0f) * expf(-d / 55.0f);
}

void span(TFT_eSPI& g, int xa, int xb, int y, uint16_t color) {
  if (xa < 0) xa = 0;
  if (xb > SCR_W - 1) xb = SCR_W - 1;
  if (xb >= xa) g.drawFastHLine(xa, y, xb - xa + 1, color);
}

void tri(TFT_eSPI& g, int x0, int y0, int x1, int y1, int x2, int y2, Rgb c) {
  g.fillTriangle(x0, y0, x1, y1, x2, y2, c565(c));
}

/* Fan from the first point; the outline must be star-shaped around it. */
void poly(TFT_eSPI& g, const int16_t (*p)[2], int n, Rgb c) {
  for (int i = 1; i + 1 < n; i++) {
    g.fillTriangle(p[0][0], p[0][1], p[i][0], p[i][1], p[i + 1][0], p[i + 1][1], c565(c));
  }
}

void mountain(TFT_eSPI& g, int left, int peakX, int peakY, int right, Rgb body, Rgb lit, Rgb snow, bool snowCap) {
  tri(g, left, HORIZON, peakX, peakY, right, HORIZON, body);
  const int ridgeX = peakX + (right - peakX) / 6;
  tri(g, peakX, peakY, left, HORIZON, ridgeX, HORIZON, lit);
  if (snowCap) {
    const float h = static_cast<float>(HORIZON - peakY);
    const float cap = h * 0.2f;
    const float sl = (peakX - left) / h;
    const float sr = (right - peakX) / h;
    tri(g, peakX, peakY, static_cast<int>(peakX - cap * sl), static_cast<int>(peakY + cap),
        static_cast<int>(peakX + cap * sr), static_cast<int>(peakY + cap), snow);
  }
}

void pine(TFT_eSPI& g, int x, int baseY, int h, Rgb body) {
  const Rgb lit = mix(body, {255, 255, 255}, 0.12f);
  for (int i = 0; i < 4; i++) {
    const int top = baseY - h + static_cast<int>(h * 0.22f * i);
    const int bottom = top + static_cast<int>(h * 0.36f);
    const int half = static_cast<int>(h * (0.10f + 0.045f * i)) + 1;
    tri(g, x, top, x - half, bottom, x + half, bottom, body);
    tri(g, x, top, x - half, bottom, x, bottom, lit);
  }
  g.fillRect(x - 1, baseY - h / 10, 3, h / 10 + 1, c565(body));
}

void cloud(TFT_eSPI& g, int x, int y, float s, Rgb c) {
  const uint16_t col = c565(c);
  g.fillCircle(x + static_cast<int>(7 * s), y, static_cast<int>(7 * s), col);
  g.fillCircle(x + static_cast<int>(17 * s), y - static_cast<int>(4 * s), static_cast<int>(9 * s), col);
  g.fillCircle(x + static_cast<int>(29 * s), y, static_cast<int>(7 * s), col);
  g.fillCircle(x + static_cast<int>(38 * s), y + static_cast<int>(2 * s), static_cast<int>(5 * s), col);
  g.fillRect(x + static_cast<int>(7 * s), y, static_cast<int>(31 * s), static_cast<int>(7 * s), col);
}

void drawSky(TFT_eSPI& g, const Palette& p) {
  for (int y = 0; y < HORIZON; y++) {
    g.drawFastHLine(0, y, SCR_W, c565(mix(p.skyTop, p.skyBottom, static_cast<float>(y) / HORIZON)));
  }
}

void drawSun(TFT_eSPI& g, const Palette& p, bool dark) {
  const int cx = 187, cy = 128;
  const Rgb sky = mix(p.skyTop, p.skyBottom, static_cast<float>(cy) / HORIZON);
  const int r = dark ? 11 : 16;
  g.fillCircle(cx, cy, r + 10, c565(mix(sky, p.sun, 0.15f)));
  g.fillCircle(cx, cy, r + 5, c565(mix(sky, p.sun, 0.4f)));
  g.fillCircle(cx, cy, r, c565(p.sun));
  if (dark) {
    static const int16_t stars[][2] = {{20, 14}, {52, 30}, {84, 12}, {150, 22}, {208, 16}, {226, 48},
                                       {12, 70}, {40, 118}, {96, 60}, {170, 54}, {222, 96}, {64, 8}};
    for (const auto& s : stars) {
      g.fillRect(s[0], s[1], 2, 2, c565(p.sun));
    }
  }
}

void drawFarLayers(TFT_eSPI& g, const Palette& p, bool dark) {
  cloud(g, 0, 44, 1.5f, p.cloud);
  cloud(g, 0, 98, 1.1f, p.cloud);
  cloud(g, 196, 52, 1.2f, p.cloud);
  cloud(g, 204, 100, 1.0f, p.cloud);

  mountain(g, 150, 228, 118, 320, p.mtn[3], p.mtnLit[3], p.snow, true);
  mountain(g, -45, 37, 94, 120, p.mtn[0], p.mtnLit[0], p.snow, true);
  mountain(g, 20, 84, 114, 160, p.mtn[1], p.mtnLit[1], p.snow, !dark);
  mountain(g, 80, 129, 130, 190, p.mtn[2], p.mtnLit[2], p.snow, false);

  g.fillRect(96, 166, SCR_W - 96, HORIZON - 166, c565(p.lake));
  g.drawFastHLine(96, 170, SCR_W - 96, c565(mix(p.lake, p.skyBottom, 0.35f)));

  static const int16_t leftRidge[][2] = {{0, HORIZON}, {0, 146}, {22, 148}, {48, 160}, {84, 171}, {118, HORIZON}};
  static const int16_t rightRidge[][2] = {{SCR_W, HORIZON}, {SCR_W, 158}, {212, 160}, {180, 167}, {150, HORIZON}};
  poly(g, leftRidge, 6, p.ridge);
  poly(g, rightRidge, 5, p.ridge);

  for (int x = 4; x < 120; x += 9) {
    pine(g, x, 172 + (x % 3), 9 + (x * 7) % 5, p.treeFar);
  }
  for (int x = 156; x < SCR_W; x += 8) {
    pine(g, x, 173, 8 + (x * 5) % 6, p.treeFar);
  }
}

void drawRoad(TFT_eSPI& g, const Palette& p, int yMax) {
  for (int y = HORIZON; y < yMax; y++) {
    const float t = static_cast<float>(y - HORIZON) / (SCR_H - HORIZON);
    const float hw = roadHalfWidth(static_cast<float>(y));
    const float cx = roadCenter(static_cast<float>(y));
    const int xl = static_cast<int>(lroundf(cx - hw));
    const int xr = static_cast<int>(lroundf(cx + hw));
    span(g, 0, xl - 1, y, c565(mix(p.vergeL0, p.vergeL1, t)));
    span(g, xr + 1, SCR_W - 1, y, c565(mix(p.vergeR0, p.vergeR1, t)));
    span(g, xl, xr, y, c565(mix(p.road0, p.road1, t)));

    const float hwNext = roadHalfWidth(static_cast<float>(y + 1));
    const float cxNext = roadCenter(static_cast<float>(y + 1));
    const int xlN = static_cast<int>(lroundf(cxNext - hwNext));
    const int xrN = static_cast<int>(lroundf(cxNext + hwNext));
    const int ew = 1 + static_cast<int>(hw * 0.03f);
    const uint16_t edge = c565(p.edge);
    const int la = xl < xlN ? xl : xlN;
    const int lb = (xl > xlN ? xl : xlN) + ew - 1;
    span(g, la, lb, y, edge);
    const int ra = (xr < xrN ? xr : xrN) - ew + 1;
    const int rb = xr > xrN ? xr : xrN;
    span(g, ra, rb, y, edge);

    if (y >= 192) {
      const float z = 400.0f / (y - 172.0f);
      if ((static_cast<int>(z * 0.5f) & 1) == 0) {
        const int dw = 1 + static_cast<int>(hw * 0.02f);
        const int dx = static_cast<int>(lroundf(cx));
        span(g, dx - dw / 2, dx + dw / 2, y, c565(p.dash));
      }
    }
  }
}

void drawRoute(TFT_eSPI& g, const Palette& p, int yMax) {
  const Rgb halo = p.glowHalo;
  for (int pass = 0; pass < 3; pass++) {
    for (int y = HORIZON + 2; y < yMax; y++) {
      const float hw = roadHalfWidth(static_cast<float>(y));
      const float hwN = roadHalfWidth(static_cast<float>(y + 1));
      const float x0 = roadCenter(static_cast<float>(y)) - 0.3f * hw;
      const float x1 = roadCenter(static_cast<float>(y + 1)) - 0.3f * hwN;
      const float core = 2.0f + hw * 0.03f;
      const float w = pass == 0 ? core + 3.0f : (pass == 1 ? core + 1.0f : core * 0.5f);
      const Rgb c = pass == 0 ? mix(p.road0, halo, 0.55f) : (pass == 1 ? p.glow : p.glowCore);
      const int xa = static_cast<int>(lroundf((x0 < x1 ? x0 : x1) - w * 0.5f));
      const int xb = static_cast<int>(lroundf((x0 > x1 ? x0 : x1) + w * 0.5f));
      span(g, xa, xb, y, c565(c));
    }
  }
}

void drawForeground(TFT_eSPI& g, const Palette& p) {
  tri(g, 0, 178, 44, 190, 0, 214, p.rock);
  tri(g, 30, 186, 80, 196, 40, 212, p.rockDark);
  tri(g, 0, 150, 30, 166, 0, 184, p.rockDark);

  pine(g, 74, 178, 22, p.treeNear);
  pine(g, 62, 182, 30, p.treeNear);
  pine(g, 48, 186, 40, p.treeNear);
  pine(g, 30, 190, 54, p.treeNear);
  pine(g, 10, 180, 78, p.treeNear);

  pine(g, 176, 174, 16, p.treeRight);
  pine(g, 188, 174, 22, p.treeRight);
  pine(g, 200, 176, 30, p.treeRight);
  pine(g, 214, 178, 40, p.treeRight);
  pine(g, 232, 180, 56, p.treeRight);
}

void drawTitle(TFT_eSPI& g, Rgb color, int cy, const char* text = "MotoPilot") {
  g.setTextDatum(MC_DATUM);
  g.setTextColor(c565(color));
  g.drawString(text, SCR_W / 2, cy, 4);
  g.drawString(text, SCR_W / 2 + 1, cy, 4);
}

void drawBluetoothGlyph(TFT_eSPI& g, int cx, int cy) {
  const uint16_t w = 0xFFFF;
  for (int dx = 0; dx < 2; dx++) {
    const int x = cx + dx;
    g.drawLine(x, cy - 9, x, cy + 9, w);
    g.drawLine(x, cy - 9, x + 5, cy - 4, w);
    g.drawLine(x + 5, cy - 4, x - 5, cy + 5, w);
    g.drawLine(x, cy + 9, x + 5, cy + 4, w);
    g.drawLine(x + 5, cy + 4, x - 5, cy - 5, w);
  }
}

void drawStatusCard(TFT_eSPI& g, const Palette& p, bool connected, const char* deviceName) {
  const int x = 16, y = 226, w = SCR_W - 32, h = 52;
  g.fillRoundRect(x, y, w, h, 10, c565(p.card));
  g.drawRoundRect(x, y, w, h, 10, c565(p.cardBorder));
  g.fillCircle(46, 252, 18, c565({20, 110, 250}));
  drawBluetoothGlyph(g, 45, 252);

  g.setTextDatum(ML_DATUM);
  g.setTextColor(c565(p.cardLabel), c565(p.card));
  g.drawString("BLUETOOTH LE", 72, 238, 1);
  g.setTextColor(0xFFFF, c565(p.card));
  g.drawString(deviceName, 72, 252, 2);
  const Rgb dot = connected ? Rgb{40, 230, 150} : Rgb{255, 190, 60};
  g.fillCircle(76, 267, 3, c565(dot));
  g.setTextColor(c565(dot), c565(p.card));
  g.drawString(connected ? "Connected" : "Waiting for phone", 86, 267, 1);
}

int16_t s_lastBarX = -1000;

}  // namespace

void drawThemeButton(TFT_eSPI& g, bool dark, int cx, int cy, int r) {
  const Palette& p = dark ? kNight : kDay;
  const float s = r / 15.0f;
  auto sc = [s](float v) { return static_cast<int>(lroundf(v * s)); };
  g.fillCircle(cx, cy, r, c565(p.btn));
  const uint16_t ic = c565(p.btnIcon);
  if (dark) {
    g.fillCircle(cx - sc(1), cy, sc(7), ic);
    g.fillCircle(cx + sc(3), cy - sc(3), sc(6), c565(p.btn));
  } else {
    g.fillCircle(cx, cy, sc(4), ic);
    for (int i = 0; i < 8; i++) {
      const float a = i * 0.7853982f;
      g.drawLine(cx + sc(cosf(a) * 7), cy + sc(sinf(a) * 7), cx + sc(cosf(a) * 10), cy + sc(sinf(a) * 10), ic);
    }
  }
}

void drawPin(TFT_eSPI& g, int x, int y, float k, uint16_t fill, uint16_t edge) {
  auto sc = [k](float v) { return static_cast<int>(lroundf(v * k)); };
  const int headY = y - sc(11);
  g.fillCircle(x, headY, sc(8), edge);
  g.fillTriangle(x - sc(7), y - sc(8), x + sc(7), y - sc(8), x, y + sc(1), edge);
  g.fillCircle(x, headY, sc(6), fill);
  g.fillTriangle(x - sc(5), y - sc(8), x + sc(5), y - sc(8), x, y - sc(1), fill);
  g.fillCircle(x, headY, sc(2), 0xFFFF);
}

void drawArrivalScreen(TFT_eSPI& tft, bool dark) {
  const Palette& p = dark ? kNight : kDay;
  drawScene(tft, dark, LOAD_ART_H);
  drawPin(tft, SCR_W / 2, 68, 2.4f, c565({234, 67, 53}), c565({165, 24, 18}));
  drawTitle(tft, p.title, 96, "You have arrived");

  tft.fillRect(0, LOAD_ART_H, SCR_W, SCR_H - LOAD_ART_H, c565(p.panel));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(c565(p.title), c565(p.panel));
  tft.drawString("Destination reached", SCR_W / 2, 270, 2);
  tft.drawString("Destination reached", SCR_W / 2 + 1, 270, 2);
  tft.setTextColor(c565(p.text2), c565(p.panel));
  tft.drawString("Navigation finished", SCR_W / 2, 292, 2);
  tft.setTextDatum(TL_DATUM);
}

void drawStatusScreen(TFT_eSPI& tft, const char* title, const char* line2, const char* line3) {
  tft.fillScreen(mapsColLand());
  if (!mapsThemeIsDark()) {
    tft.fillCircle(SCR_W - 30, 90, 36, mapsColPark());
  }
  tft.setTextDatum(MC_DATUM);

  tft.fillRect(0, 0, SCR_W, 36, mapsColCard());
  tft.fillRect(0, 36, SCR_W, 2, mapsColCardShadow());
  tft.setTextColor(mapsColText(), mapsColCard());
  tft.drawString(title, SCR_W / 2, 18, 2);

  drawLogo(tft, SCR_W / 2, 69, 52);

  tft.fillRoundRect(16, 100, SCR_W - 32, 80, 8, mapsColCard());
  tft.fillRect(16, 100, SCR_W - 32, 3, mapsColRoute());

  if (line2 != nullptr) {
    tft.setTextColor(mapsColAccent(), mapsColCard());
    tft.drawString(line2, SCR_W / 2, 128, 2);
  }
  if (line3 != nullptr) {
    tft.setTextColor(mapsColMuted(), mapsColCard());
    tft.drawString(line3, SCR_W / 2, 156, 2);
  }

  drawThemeButton(tft, mapsThemeIsDark(), 216, 300, 15);

  tft.setTextColor(mapsColMuted(), mapsColLand());
  tft.drawString("Tap: light / dark", SCR_W / 2, SCR_H - 16, 1);
}

void drawScene(TFT_eSPI& tft, bool dark, int yMax) {
  const Palette& p = dark ? kNight : kDay;
  drawSky(tft, p);
  drawSun(tft, p, dark);
  drawFarLayers(tft, p, dark);
  drawRoad(tft, p, yMax);
  drawRoute(tft, p, yMax);
  drawForeground(tft, p);
}

void drawHomeScreen(TFT_eSPI& tft, bool dark, bool connected, const char* deviceName) {
  const Palette& p = dark ? kNight : kDay;
  drawScene(tft, dark, SCR_H);
  drawLogo(tft, SCR_W / 2, 43, 60);
  drawTitle(tft, p.title, 87);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(c565(p.subtitle));
  tft.drawString("Ready to navigate", SCR_W / 2, 107, 2);
  drawStatusCard(tft, p, connected, deviceName);
  drawThemeButton(tft, dark, 216, 300, 15);
  tft.setTextDatum(TL_DATUM);
}

void drawLoadingScreen(TFT_eSPI& tft, bool dark) {
  const Palette& p = dark ? kNight : kDay;
  drawScene(tft, dark, LOAD_ART_H);
  drawLogo(tft, SCR_W / 2, 52, 60);
  drawTitle(tft, p.title, 96);

  tft.fillRect(0, LOAD_ART_H, SCR_W, SCR_H - LOAD_ART_H, c565(p.panel));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(c565(p.title), c565(p.panel));
  tft.drawString("Getting directions...", SCR_W / 2, 277, 2);
  tft.drawString("Getting directions...", SCR_W / 2 + 1, 277, 2);
  tft.setTextColor(c565(p.text2), c565(p.panel));
  tft.drawString("Loading map data", SCR_W / 2, 297, 2);
  tft.setTextDatum(TL_DATUM);

  drawLoadingBar(tft, dark, 0, true);
}

void drawLoadingBar(TFT_eSPI& tft, bool dark, uint32_t nowMs, bool force) {
  const Palette& p = dark ? kNight : kDay;
  const int barX = 20, barW = SCR_W - 40, barY = 250, barH = 6, segW = 76;
  const float t = static_cast<float>(nowMs % 1500U) / 1500.0f;
  const float e = t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
  const int16_t a = static_cast<int16_t>(-segW + e * (barW + segW));
  if (!force && (a - s_lastBarX < 2 && s_lastBarX - a < 2)) {
    return;
  }
  s_lastBarX = a;

  tft.fillRoundRect(barX, barY, barW, barH, barH / 2, c565(p.track));
  const int from = a < 0 ? 0 : a;
  const int to = (a + segW) > barW ? barW : (a + segW);
  if (to - from >= 2) {
    tft.fillRoundRect(barX + from, barY, to - from, barH, barH / 2, c565(p.bar));
  }
}
