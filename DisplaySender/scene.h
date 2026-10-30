#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

/** Landscape backdrop shared by the home and loading screens; rows below yMax are left untouched. */
void drawScene(TFT_eSPI& tft, bool dark, int yMax);

/** Boot and error screen: title plus up to two lines of text in a card. */
void drawStatusScreen(TFT_eSPI& tft, const char* title, const char* line2, const char* line3);

/** Round sun/moon theme button centred on (cx, cy). */
void drawThemeButton(TFT_eSPI& tft, bool dark, int cx, int cy, int r);

/** Home screen shown while no phone is connected. */
void drawHomeScreen(TFT_eSPI& tft, bool dark, bool connected, const char* deviceName);

/** Loading screen shown until the first navigation frame arrives. */
void drawLoadingScreen(TFT_eSPI& tft, bool dark);

/** Redraws the indeterminate progress bar; no-op when the segment has not moved. */
void drawLoadingBar(TFT_eSPI& tft, bool dark, uint32_t nowMs, bool force);

/** Screen shown when the destination is reached. */
void drawArrivalScreen(TFT_eSPI& tft, bool dark);

/** Red map pin with its tip on (x, y); scale 1 is about 21 px tall. Also used on the map. */
void drawPin(TFT_eSPI& tft, int x, int y, float scale, uint16_t fill, uint16_t edge);
