"""Generates res/app.ico (rounded dark square + ice-blue microphone).  Needs Pillow: pip install pillow"""
import os
from PIL import Image, ImageDraw
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

BG, EDGE, ICE = (16, 24, 32, 255), (36, 51, 66, 255), (111, 145, 168, 255)

def render(size):
    S = 1024
    k = S / 32.0
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=int(S * 0.22), fill=BG, outline=EDGE, width=int(S * 0.012))
    # mic glyph in a 32x32 box, centered in a 24x24 area
    sc, ox, oy = 0.78, 16 - 16 * 0.78, 16 - 16 * 0.78 - 0.4
    def P(x, y): return ((x * sc + ox) * k, (y * sc + oy) * k)
    def line(a, b, w):
        (x1, y1), (x2, y2) = P(*a), P(*b)
        r = w * sc * k / 2
        d.line((x1, y1, x2, y2), fill=ICE, width=int(w * sc * k))
        for x, y in ((x1, y1), (x2, y2)): d.ellipse((x - r, y - r, x + r, y + r), fill=ICE)
    (x1, y1), (x2, y2) = P(11, 3), P(21, 18)
    d.rounded_rectangle((x1, y1, x2, y2), radius=5 * sc * k, fill=ICE)
    (ax1, ay1), (ax2, ay2) = P(7, 5), P(25, 23)
    d.arc((ax1, ay1, ax2, ay2), 0, 180, fill=ICE, width=int(2.4 * sc * k))
    line((16, 23), (16, 27), 2.4)
    line((11.5, 27.5), (20.5, 27.5), 2.4)
    return img.resize((size, size), Image.LANCZOS)

big = render(256)
big.save(os.path.join(ROOT, "res", "app.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (256, 256)])
print("ok")
