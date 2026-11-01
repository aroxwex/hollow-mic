#include "draw.h"
#include <map>
using namespace Gdiplus;

namespace draw {

static ULONG_PTR g_token = 0;

void Init() {
    GdiplusStartupInput in;
    GdiplusStartup(&g_token, &in, nullptr);
}
void Shutdown() { GdiplusShutdown(g_token); }

Color Rgb(DWORD c, BYTE a) { return Color(a, (c >> 16) & 255, (c >> 8) & 255, c & 255); }

Color Lerp(DWORD a, DWORD b, float t) {
    auto ch = [&](int sh) {
        int x = (a >> sh) & 255, y = (b >> sh) & 255;
        return (BYTE)(x + (y - x) * t + 0.5f);
    };
    return Color(255, ch(16), ch(8), ch(0));
}

static void RoundPath(GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

void FillRound(Graphics& g, float x, float y, float w, float h, float r, const Color& c) {
    GraphicsPath p;
    RoundPath(p, x, y, w, h, r);
    SolidBrush b(c);
    g.FillPath(&b, &p);
}

void StrokeRound(Graphics& g, float x, float y, float w, float h, float r, const Color& c, float width) {
    GraphicsPath p;
    RoundPath(p, x, y, w, h, r);
    Pen pen(c, width);
    g.DrawPath(&pen, &p);
}

static FontFamily* Family() {
    static FontFamily* f = nullptr;
    if (!f) {
        f = new FontFamily(L"Segoe UI");
        if (!f->IsAvailable()) {
            delete f;
            f = FontFamily::GenericSansSerif()->Clone();
        }
    }
    return f;
}

static Font* GetFont(float size, bool bold) {
    static std::map<int, Font*> cache;
    int key = (int)(size * 10) * 2 + (bold ? 1 : 0);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Font* f = new Font(Family(), size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    cache[key] = f;
    return f;
}

void Text(Graphics& g, const std::wstring& s, float x, float y, float w, float h,
          float size, bool bold, DWORD rgb, int align) {
    StringFormat fmt(StringFormat::GenericTypographic());
    fmt.SetAlignment(align == 0 ? StringAlignmentNear : align == 1 ? StringAlignmentCenter : StringAlignmentFar);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush br(Rgb(rgb));
    g.DrawString(s.c_str(), -1, GetFont(size, bold), RectF(x, y, w, h), &fmt, &br);
}

void Mic(Graphics& g, float x, float y, float size, DWORD rgb, bool muted) {
    GraphicsState st = g.Save();
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(x, y);
    g.ScaleTransform(size / 32.0f, size / 32.0f);
    SolidBrush br(Rgb(rgb));
    Pen pen(Rgb(rgb), 2.3f);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    // capsule
    GraphicsPath cap;
    RoundPath(cap, 11, 3, 10, 15, 5);
    g.FillPath(&br, &cap);
    // holder arc, stem, base
    g.DrawArc(&pen, 7.0f, 5.0f, 18.0f, 18.0f, 0.0f, 180.0f);
    g.DrawLine(&pen, 16.0f, 23.0f, 16.0f, 27.0f);
    g.DrawLine(&pen, 11.5f, 27.5f, 20.5f, 27.5f);
    if (muted) {
        Pen slash(Rgb(rgb), 2.6f);
        slash.SetStartCap(LineCapRound);
        slash.SetEndCap(LineCapRound);
        g.DrawLine(&slash, 5.5f, 5.0f, 26.5f, 28.0f);
    }
    g.Restore(st);
}

HICON MakeMicIcon(int px, DWORD rgb, bool muted) {
    Bitmap bmp(px, px, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    Mic(g, 0, 0, (float)px, rgb, muted);
    HICON h = nullptr;
    bmp.GetHICON(&h);
    return h;
}

}  // namespace draw
