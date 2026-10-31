#pragma once
// Host-side stand-in for TFT_eSPI: rasterises into a framebuffer so screens can be previewed on a PC.
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <math.h>
#include <algorithm>

#include "Arduino.h"

#define TL_DATUM 0
#define ML_DATUM 3
#define MC_DATUM 4

struct TextOp {
  int x, y, font, datum;
  uint16_t color;
  std::string text;
};

class TFT_eSPI {
 public:
  int W = 240, H = 320;
  std::vector<uint16_t> fb = std::vector<uint16_t>(240 * 320, 0);
  std::vector<TextOp> texts;
  int datum = 0;
  uint16_t fg = 0xFFFF;

  void put(int x, int y, uint16_t c) {
    if (x >= 0 && x < W && y >= 0 && y < H) fb[y * W + x] = c;
  }
  void drawPixel(int x, int y, uint16_t c) { put(x, y, c); }
  void fillScreen(uint16_t c) { std::fill(fb.begin(), fb.end(), c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) put(x + i, y + j, c);
  }
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
      put(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  void fillCircle(int cx, int cy, int r, uint16_t c) {
    for (int y = -r; y <= r; y++)
      for (int x = -r; x <= r; x++)
        if (x * x + y * y <= r * r) put(cx + x, cy + y, c);
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    const int minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
    const int miny = std::min({y0, y1, y2}), maxy = std::max({y0, y1, y2});
    auto edge = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
    for (int y = miny; y <= maxy; y++)
      for (int x = minx; x <= maxx; x++) {
        const int a = edge(x0, y0, x1, y1, x, y), b = edge(x1, y1, x2, y2, x, y), d = edge(x2, y2, x0, y0, x, y);
        if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) put(x, y, c);
      }
  }
  static bool inRound(int px, int py, int w, int h, int r) {
    const int cx = px < r ? r : (px >= w - r ? w - r - 1 : px);
    const int cy = py < r ? r : (py >= h - r ? h - r - 1 : py);
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (inRound(i, j, w, h, r)) put(x + i, y + j, c);
  }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (inRound(i, j, w, h, r) && !inRound(i - 1, j - 1, w - 2, h - 2, r - 1)) put(x + i, y + j, c);
  }
  // 1 bit per pixel, rows padded to whole bytes, most significant bit first.
  void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, uint16_t c) {
    const int byteWidth = (w + 7) / 8;
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (bitmap[j * byteWidth + i / 8] & (128 >> (i & 7))) put(x + i, y + j, c);
  }
  void setTextDatum(int d) { datum = d; }
  void setTextColor(uint16_t f) { fg = f; }
  void setTextColor(uint16_t f, uint16_t) { fg = f; }
  void drawString(const char* s, int x, int y, int font) { texts.push_back({x, y, font, datum, fg, s}); }
  // Rough widths of the built-in fonts; the real panel measures the glyphs.
  int textWidth(const char* s, int font) const {
    const int perChar = font == 6 ? 30 : (font == 4 ? 14 : (font == 2 ? 8 : 6));
    return static_cast<int>(strlen(s)) * perChar;
  }
  void dump(const char* ppm, const char* txt) const {
    FILE* f = fopen(ppm, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (uint16_t c : fb) {
      const unsigned char px[3] = {static_cast<unsigned char>((c >> 8) & 0xF8), static_cast<unsigned char>((c >> 3) & 0xFC),
                                   static_cast<unsigned char>((c << 3) & 0xF8)};
      fwrite(px, 1, 3, f);
    }
    fclose(f);
    FILE* t = fopen(txt, "w");
    for (const auto& o : texts) fprintf(t, "%d\t%d\t%d\t%d\t%d\t%s\n", o.x, o.y, o.font, o.datum, o.color, o.text.c_str());
    fclose(t);
  }
};

class TFT_eSprite : public TFT_eSPI {
 public:
  explicit TFT_eSprite(TFT_eSPI* parentDisplay) : parent(parentDisplay) {}
  void setColorDepth(int d) { depth = d; }
  void* createSprite(int w, int h) {
    W = w;
    H = h;
    fb.assign(static_cast<size_t>(w) * h, 0);
    return fb.data();
  }
  void createPalette(const uint16_t* colors, int count) { palette.assign(colors, colors + count); }
  void fillSprite(uint16_t c) {
    std::fill(fb.begin(), fb.end(), c);
    texts.clear();
  }
  void pushSprite(int x, int y) { pushSprite(x, y, 0, 0, W, H); }
  void pushSprite(int x, int y, int sx, int sy, int w, int h) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) {
        uint16_t v = fb[(sy + j) * W + sx + i];
        if (depth == 4 && !palette.empty()) v = palette[v & 15];
        parent->put(x + i, y + j, v);
      }
    for (TextOp t : texts) {
      t.x += x - sx;
      t.y += y - sy;
      parent->texts.push_back(t);
    }
  }

 private:
  TFT_eSPI* parent;
  int depth = 16;
  std::vector<uint16_t> palette;
};
