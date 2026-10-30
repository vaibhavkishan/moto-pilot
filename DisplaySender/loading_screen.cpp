#include "loading_screen.h"
#include "scene.h"
#include "maps_theme.h"

void showMapLoadingScreen(TFT_eSPI& tft) {
  drawLoadingScreen(tft, mapsThemeIsDark());
}

void updateMapLoadingAnimation(TFT_eSPI& tft, uint32_t nowMs) {
  drawLoadingBar(tft, mapsThemeIsDark(), nowMs, false);
}
