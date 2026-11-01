#pragma once
#include <windows.h>
#include <algorithm>
#include <string>
using std::min;
using std::max;
#include <gdiplus.h>

namespace draw {
void Init();
void Shutdown();

Gdiplus::Color Rgb(DWORD rgb, BYTE a = 255);
Gdiplus::Color Lerp(DWORD a, DWORD b, float t);

void FillRound(Gdiplus::Graphics& g, float x, float y, float w, float h, float r, const Gdiplus::Color& c);
void StrokeRound(Gdiplus::Graphics& g, float x, float y, float w, float h, float r, const Gdiplus::Color& c, float width);

// align: 0 left, 1 center, 2 right
void Text(Gdiplus::Graphics& g, const std::wstring& s, float x, float y, float w, float h,
          float size, bool bold, DWORD rgb, int align = 0);

// microphone glyph in a size x size box at (x,y)
void Mic(Gdiplus::Graphics& g, float x, float y, float size, DWORD rgb, bool muted);
HICON MakeMicIcon(int px, DWORD rgb, bool muted);
}
