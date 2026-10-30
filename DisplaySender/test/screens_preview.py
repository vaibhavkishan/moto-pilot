"""Turns the PPM/TXT pairs written by screens_preview into PNGs (text drawn with a system font) and a contact sheet."""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
SCALE = 2
# Approximate pixel heights of the TFT_eSPI built-in fonts 1, 2 and 4.
FONT_PX = {1: 8, 2: 14, 4: 24}
ANCHOR = {4: "mm", 3: "lm", 0: "la"}
ORDER = ["boot", "home", "loading", "nav_turn", "nav_rerouting", "nav_weak_gps", "nav_arriving", "arrival"]


def font(size):
    for name in ("arialbd.ttf", "arial.ttf", "DejaVuSans-Bold.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def rgb565(value):
    return ((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31)


def render(name, theme):
    base = f"{src}/{name}_{theme}"
    image = Image.open(base + ".ppm").convert("RGB")
    image = image.resize((image.width * SCALE, image.height * SCALE), Image.NEAREST)
    draw = ImageDraw.Draw(image)
    for line in open(base + ".txt", encoding="utf-8"):
        x, y, f, datum, color, text = line.rstrip("\n").split("\t")
        draw.text((int(x) * SCALE, int(y) * SCALE), text, fill=rgb565(int(color)),
                  font=font(FONT_PX[int(f)] * SCALE), anchor=ANCHOR[int(datum)])
    return image


sheet_w = 240 * SCALE
sheet = Image.new("RGB", ((sheet_w + 12) * len(ORDER), (320 * SCALE + 12) * 2), (255, 255, 255))
for row, theme in enumerate(("light", "dark")):
    for col, name in enumerate(ORDER):
        image = render(name, theme)
        image.save(f"{out}/{name}_{theme}.png")
        sheet.paste(image, (col * (sheet_w + 12), row * (320 * SCALE + 12)))
sheet.save(f"{out}/all_screens.png")
print("ok", len(ORDER) * 2)
