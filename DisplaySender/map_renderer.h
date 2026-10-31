#pragma once

#include <TFT_eSPI.h>
#include "nav_types.h"
#include "map_transform.h"

class MapRenderer {
public:
  explicit MapRenderer(TFT_eSPI& display);

  /** Allocate the off-screen buffers. Call after BLE init so heap use is realistic. */
  bool begin();

  /** Force the next draw() to repaint map and bottom sheet (screen was used by something else). */
  void invalidate();

  /** Compose the map off-screen and push it once; repaint only the bottom-sheet text that changed. */
  void draw(const NavState& state, const MapView& view);

  void drawThemeSwitch();
  void drawThemeSwitchWaiting();
  static bool themeSwitchHit(uint16_t x, uint16_t y);

  uint32_t lastDrawUs() const { return lastDrawUs_; }
  bool usesSprite() const { return mapSprOk_; }

private:
  static const int LINE_W = 224;  // widest bottom-sheet line
  static const int LINE_H = 28;   // tallest bottom-sheet line (font 4)
  static const int LINE_COUNT = 4;
  static const int ICON_SIZE = 48;

  enum LineKind { LINE_UTF8, LINE_FONT4, LINE_FONT2 };

  TFT_eSPI& tft;
  TFT_eSprite mapSpr_;
  TFT_eSprite lineSpr_;
  TFT_eSprite iconSpr_;
  TFT_eSprite distSpr_;
  bool mapSprOk_ = false;
  bool lineSprOk_ = false;
  bool iconSprOk_ = false;
  bool distSprOk_ = false;
  bool lastMinimal_ = false;
  char minimalText_[24] = {};
  int minimalProgressPx_ = -2;

  bool paletteValid_ = false;
  bool paletteDark_ = false;

  bool overlayValid_ = false;
  bool overlayDark_ = false;
  bool overlayHtml_ = false;
  bool iconValid_ = false;
  uint8_t iconShown_ = 0;
  char htmlCache_[NAV_HTML_MAX] = {};
  char lineCache_[LINE_COUNT][130] = {};
  uint16_t lineColor_[LINE_COUNT] = {};

  uint32_t lastDrawUs_ = 0;

  void drawMapLayer(const NavState& state, const MapView& view);
  void drawOverlay(const NavState& state);
  void drawOverlayBase();
  void drawOverlayMinimal(const NavState& state);
  void drawTextLine(int index, int x, int y, int width, int height, const char* text, uint16_t color,
                    LineKind kind);
  void drawManeuverIcon(uint8_t maneuver, uint16_t foreground, uint16_t background);
  void drawThemeSwitchAt(int sx, int sy);
};
