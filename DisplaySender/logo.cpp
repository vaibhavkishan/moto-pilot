#include "logo.h"

#include <math.h>

static uint16_t rgb(int r, int g, int b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void drawLogo(TFT_eSPI& tft, int cx, int cy, int size) {
  const int half = size / 2;
  const int radius = size / 4;
  const int top = cy - half;

  // Vertical gradient tile; rows near the top and bottom are inset to round the corners.
  for (int row = 0; row < size; row++) {
    const float t = static_cast<float>(row) / (size - 1);
    const int r = static_cast<int>(40 + (14 - 40) * t);
    const int g = static_cast<int>(128 + (78 - 128) * t);
    const int b = static_cast<int>(255 + (222 - 255) * t);
    float dy = 0.0f;
    if (row < radius) {
      dy = radius - row - 0.5f;
    } else if (row >= size - radius) {
      dy = row - (size - radius) + 0.5f;
    }
    const int inset = dy > 0.0f ? static_cast<int>(lroundf(radius - sqrtf(radius * radius - dy * dy))) : 0;
    tft.drawFastHLine(cx - half + inset, top + row, size - 2 * inset, rgb(r, g, b));
  }

  // Notched arrowhead: lit left facet, shaded right facet.
  const float k = static_cast<float>(size) * 0.64f / 24.0f;
  const int tipX = cx, tipY = cy - static_cast<int>(12.0f * k);
  const int leftX = cx - static_cast<int>(10.0f * k), baseY = cy + static_cast<int>(12.0f * k);
  const int rightX = cx + static_cast<int>(10.0f * k);
  const int notchY = cy + static_cast<int>(6.0f * k);
  tft.fillTriangle(tipX, tipY, leftX, baseY, cx, notchY, rgb(238, 245, 255));
  tft.fillTriangle(tipX, tipY, rightX, baseY, cx, notchY, rgb(160, 196, 248));
}
