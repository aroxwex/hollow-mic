#include "ui.h"
#include "theme.h"
#include <windowsx.h>
#include <cmath>
#include <dwmapi.h>
using namespace Gdiplus;
using draw::Rgb;
using draw::Lerp;

namespace ui {

float g_scale = 1.0f;

void InitScale() {
    HDC dc = GetDC(nullptr);
    g_scale = GetDeviceCaps(dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(nullptr, dc);
    if (g_scale < 1.0f) g_scale = 1.0f;
}

static const wchar_t* kClass = L"HollowMicPanel";
static constexpr float kRadius = 12.0f;
static constexpr int kItemH = 32;

Panel::Panel(int w, int h, std::wstring title, bool titleBar, bool topmost)
    : w_(w), h_(h), titleBar_(titleBar), topmost_(topmost), title_(std::move(title)) {}
Panel::~Panel() { Destroy(); }

Widget& Panel::Add(const Widget& w) {
    ws_.push_back(w);
    return ws_.back();
}

Widget* Panel::Find(int id) {
    for (auto& w : ws_) if (w.id == id) return &w;
    return nullptr;
}

void Panel::SnapAnimations() {
    for (auto& w : ws_) {
        w.hover = w.hoverTarget;
        w.onAnim = w.on ? 1.0f : 0.0f;
    }
}

bool Panel::Create() {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = StaticProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        wc.hIconSm = wc.hIcon;
        wc.lpszClassName = kClass;
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }
    scale_ = g_scale;
    int W = (int)(w_ * scale_ + 0.5f), H = (int)(h_ * scale_ + 0.5f);
    hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | (topmost_ ? WS_EX_TOPMOST : 0), kClass, title_.c_str(), WS_POPUP, 0, 0, W, H,
                           nullptr, nullptr, inst, this);
    if (!hwnd) return false;
    ApplyCorners();
    return true;
}

void Panel::ApplyCorners() {
    int W = (int)(w_ * scale_ + 0.5f), H = (int)(h_ * scale_ + 0.5f);
    int pref = 2;  // DWMWCP_ROUND (Windows 11); fails on Windows 10
    bool dwmOk = SUCCEEDED(DwmSetWindowAttribute(hwnd, 33, &pref, sizeof(pref)));
    if (!dwmOk) {  // Windows 10: clip the window to a rounded region
        int r = (int)(kRadius * scale_ * 2);
        SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, W + 1, H + 1, r, r), TRUE);
    }
}

void Panel::Destroy() {
    if (hwnd) {
        HWND h = hwnd;
        DestroyWindow(h);
        hwnd = nullptr;
    }
}

void Panel::Show() {
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    Invalidate();
}
void Panel::Hide() { if (hwnd) ShowWindow(hwnd, SW_HIDE); }
bool Panel::Visible() const { return hwnd && IsWindowVisible(hwnd); }
void Panel::Invalidate() { if (hwnd) InvalidateRect(hwnd, nullptr, FALSE); }

void Panel::MoveBottomRight() {
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int m = (int)(16 * scale_);
    SetWindowPos(hwnd, nullptr, wa.right - (rc.right - rc.left) - m, wa.bottom - (rc.bottom - rc.top) - m,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Panel::MoveTo(int x, int y) {
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Panel::MoveCenter() {
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    SetWindowPos(hwnd, nullptr, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK Panel::StaticProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    Panel* p;
    if (m == WM_NCCREATE) {
        p = (Panel*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)p);
        p->hwnd = h;
    } else {
        p = (Panel*)GetWindowLongPtrW(h, GWLP_USERDATA);
    }
    if (!p) return DefWindowProcW(h, m, wp, lp);
    return p->Proc(m, wp, lp);
}

static bool Inside(const RECT& r, float x, float y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

Panel::Hit Panel::HitTest(float x, float y) {
    Hit h;
    // open selector list sits on top of everything
    for (int i = (int)ws_.size() - 1; i >= 0; i--) {
        Widget& w = ws_[i];
        if (w.kind == Kind::Selector && w.open) {
            RECT list = {w.r.left, w.r.bottom + 4, w.r.right, w.r.bottom + 4 + 8 + (LONG)w.items.size() * kItemH};
            if (Inside(list, x, y)) {
                int idx = (int)((y - list.top - 4) / kItemH);
                if (idx >= 0 && idx < (int)w.items.size()) { h.w = i; h.item = idx; return h; }
                h.w = i; h.item = -2;   // padding of the list: swallow the click
                return h;
            }
        }
    }
    for (int i = (int)ws_.size() - 1; i >= 0; i--) {
        Widget& w = ws_[i];
        if (!w.visible || w.kind == Kind::Label || w.kind == Kind::Separator) continue;
        if (Inside(w.r, x, y)) { h.w = i; return h; }
    }
    return h;
}

void Panel::CloseSelectors() {
    for (auto& w : ws_) if (w.kind == Kind::Selector && w.open) { w.open = false; w.hotItem = -1; }
}

void Panel::StartAnim() {
    if (!timer_ && hwnd) { SetTimer(hwnd, 1, 16, nullptr); timer_ = true; }
}

void Panel::Animate() {
    bool active = false;
    for (auto& w : ws_) {
        float d = w.hoverTarget - w.hover;
        if (std::fabs(d) > 0.02f) { w.hover += d * 0.38f; active = true; } else w.hover = w.hoverTarget;
        float t = w.on ? 1.0f : 0.0f;
        float d2 = t - w.onAnim;
        if (std::fabs(d2) > 0.02f) { w.onAnim += d2 * 0.34f; active = true; } else w.onAnim = t;
    }
    Invalidate();
    if (!active && timer_) { KillTimer(hwnd, 1); timer_ = false; }
}

LRESULT Panel::Proc(UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        Paint(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;

    case WM_NCHITTEST: {
        POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd, &p);
        float x = p.x / scale_, y = p.y / scale_;
        Hit h = HitTest(x, y);
        if (h.w >= 0) return HTCLIENT;
        if (titleBar_ && y < kTitleH) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lp) == HTCLIENT) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            Hit h = HitTest(p.x / scale_, p.y / scale_);
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(h.w >= 0 ? 32649 : 32512)));
            return TRUE;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (!tracking_) {
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&t);
            tracking_ = true;
        }
        Hit h = HitTest(GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_);
        for (int i = 0; i < (int)ws_.size(); i++) {
            Widget& w = ws_[i];
            bool hot = (i == h.w) && h.item != -2;
            if (w.kind == Kind::Selector && w.open) {
                bool overList = (i == h.w && h.item >= 0);
                bool overBox = hot && h.item == -1;
                w.hoverTarget = (overBox || overList) ? 1.0f : 0.0f;
                int hi = overList ? h.item : -1;
                if (hi != w.hotItem) { w.hotItem = hi; Invalidate(); }
            } else {
                w.hoverTarget = hot ? 1.0f : 0.0f;
            }
        }
        StartAnim();
        return 0;
    }
    case WM_MOUSELEAVE: {
        tracking_ = false;
        for (auto& w : ws_) { w.hoverTarget = 0; w.hotItem = -1; }
        StartAnim();
        Invalidate();
        return 0;
    }

    case WM_LBUTTONDOWN: {
        Hit h = HitTest(GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_);
        pressed_ = h.w;
        bool onOpenList = false;
        if (h.w >= 0 && ws_[h.w].kind == Kind::Selector && ws_[h.w].open && h.item != -1) onOpenList = true;
        if (!onOpenList) {
            // click outside an open selector closes it (and is swallowed)
            for (int i = 0; i < (int)ws_.size(); i++) {
                if (ws_[i].kind == Kind::Selector && ws_[i].open && i != h.w) {
                    CloseSelectors();
                    pressed_ = -2;
                    Invalidate();
                    return 0;
                }
            }
        }
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (GetCapture() == hwnd) ReleaseCapture();
        Hit h = HitTest(GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_);
        int pr = pressed_;
        pressed_ = -1;
        if (h.w < 0 || h.w != pr) return 0;
        Widget& w = ws_[h.w];
        switch (w.kind) {
        case Kind::Selector:
            if (w.open && h.item >= 0) {
                w.sel = h.item;
                w.open = false;
                w.hotItem = -1;
                Invalidate();
                if (onSelect) onSelect(w.id, h.item);
            } else if (h.item == -1) {
                w.open = !w.open;
                Invalidate();
            }
            break;
        case Kind::Toggle:
            w.on = !w.on;
            StartAnim();
            if (onClick) onClick(w.id);
            break;
        case Kind::Button: case Kind::IconBtn: case Kind::Status: case Kind::Box: case Kind::MenuItem:
            if (onClick) onClick(w.id);
            break;
        default: break;
        }
        return 0;
    }

    case WM_TIMER:
        if (wp == 1) Animate();
        return 0;

    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (onKey && onKey(m, wp, lp)) return 0;
        if (wp == VK_ESCAPE) {
            bool any = false;
            for (auto& w : ws_) if (w.kind == Kind::Selector && w.open) any = true;
            if (any) { CloseSelectors(); Invalidate(); return 0; }
        }
        break;
    case WM_SYSCHAR: return 0;   // no beep on Alt+key

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && onDeactivate) onDeactivate();
        break;

    case WM_CLOSE:
        if (onCloseRequest) onCloseRequest(); else Hide();
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        timer_ = false;
        break;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
}

// ---------------------------------------------------------------- painting

void Panel::Paint(HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return;
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.Clear(Rgb(theme::bg));
        g.ScaleTransform(scale_, scale_);

        if (titleBar_) {
            SolidBrush bar(Rgb(theme::bar));
            g.FillRectangle(&bar, 0, 0, w_, kTitleH);
            draw::Mic(g, 13, 9, 18, theme::ice, false);
            draw::Text(g, title_, 38, 0, (float)w_ - 160, (float)kTitleH, 13, true, theme::silver, 0);
        }

        for (auto& w : ws_) if (w.visible) DrawWidget(g, w);
        for (auto& w : ws_) if (w.kind == Kind::Selector && w.open) DrawSelectorList(g, w);

        draw::StrokeRound(g, 0.5f, 0.5f, (float)w_ - 1, (float)h_ - 1, kRadius, Rgb(theme::line), 1.0f);
    }
    BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void Line(Graphics& g, DWORD rgb, float width, float x1, float y1, float x2, float y2) {
    Pen p(Rgb(rgb), width);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, x1, y1, x2, y2);
}

void Panel::DrawWidget(Graphics& g, Widget& w) {
    float x = (float)w.r.left, y = (float)w.r.top;
    float cw = (float)(w.r.right - w.r.left), ch = (float)(w.r.bottom - w.r.top);
    switch (w.kind) {
    case Kind::Label:
        draw::Text(g, w.text, x, y, cw, ch, w.font, w.bold, w.fg, w.align);
        break;

    case Kind::MenuItem: {
        draw::FillRound(g, x, y, cw, ch, 7, Lerp(w.bg, w.bgHover, w.hover));
        draw::Text(g, w.text, x + 14, y, cw - 28, ch, w.font, false, w.fg, 0);
        if (!w.text2.empty()) draw::Text(g, w.text2, x + 14, y, cw - 28, ch, 12, false, theme::dim, 2);
        break;
    }

    case Kind::Separator: {
        SolidBrush sb(Rgb(theme::line));
        g.FillRectangle(&sb, x + 10, std::floor(y + ch / 2), cw - 20, 1.0f);
        break;
    }

    case Kind::Button: {
        Color fill = Lerp(w.bg, w.bgHover, w.hover);
        draw::FillRound(g, x, y, cw, ch, 9, fill);
        if (w.outline) draw::StrokeRound(g, x + 0.5f, y + 0.5f, cw - 1, ch - 1, 9, Lerp(theme::line, theme::ice, 0.4f + 0.6f * w.hover), 1.2f);
        draw::Text(g, w.text, x, y, cw, ch, w.font, w.bold, w.fg, 1);
        break;
    }

    case Kind::IconBtn: {
        SolidBrush b(Lerp(w.bg, w.bgHover, w.hover));
        g.FillRectangle(&b, x, y, cw, ch);
        float cx = x + cw / 2, cy = y + ch / 2;
        DWORD c = theme::silver;
        if (w.glyph == Glyph::Minimize) {
            SolidBrush lb(Rgb(c));
            g.FillRectangle(&lb, std::floor(cx - 5), std::floor(cy), 10.0f, 1.0f);
        }
        else if (w.glyph == Glyph::Close) {
            Line(g, c, 1.4f, cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f);
            Line(g, c, 1.4f, cx - 4.5f, cy + 4.5f, cx + 4.5f, cy - 4.5f);
        } else if (w.glyph == Glyph::Help) draw::Text(g, L"?", x, y, cw, ch, 14, true, c, 1);
        break;
    }

    case Kind::Status: {
        draw::FillRound(g, x, y, cw, ch, 14, Lerp(w.bg, w.bgHover, w.hover));
        DWORD state = w.muted ? theme::red : theme::silver;
        float d = 42;
        float cx = x + 16, cy = y + (ch - d) / 2;
        draw::FillRound(g, cx, cy, d, d, d / 2, Lerp(theme::bg, theme::bar, 0.5f));
        draw::Mic(g, cx + 9, cy + 9, 24, state, w.muted);
        draw::Text(g, w.text, x + 72, y + 12, cw - 84, 24, 17, true, state, 0);
        draw::Text(g, w.text2, x + 72, y + 36, cw - 84, 18, 11.5f, false, theme::dim, 0);
        break;
    }

    case Kind::Box: {
        draw::FillRound(g, x, y, cw, ch, 9, Lerp(w.bg, w.bgHover, w.hover));
        draw::Text(g, w.text, x + 14, y, cw - 28, ch, w.font, w.bold, w.fg, 0);
        break;
    }

    case Kind::Toggle: {
        DWORD lc = w.fg;
        Color lab = Lerp(lc, 0xFFFFFF, w.hover * 0.25f);
        SolidBrush lb(lab);
        {
            StringFormat fmt(StringFormat::GenericTypographic());
            fmt.SetLineAlignment(StringAlignmentCenter);
            fmt.SetTrimming(StringTrimmingEllipsisCharacter);
            fmt.SetFormatFlags(StringFormatFlagsNoWrap);
            static Font* f = nullptr;
            if (!f) {
                FontFamily fam(L"Segoe UI");
                if (!fam.IsAvailable()) f = new Font(FontFamily::GenericSansSerif(), w.font, FontStyleRegular, UnitPixel);
                else f = new Font(&fam, w.font, FontStyleRegular, UnitPixel);
            }
            g.DrawString(w.text.c_str(), -1, f, RectF(x, y, cw - 56, ch), &fmt, &lb);
        }
        float tw = 40, th = 22;
        float tx = x + cw - tw, ty = y + (ch - th) / 2;
        draw::FillRound(g, tx, ty, tw, th, th / 2, Lerp(theme::panel2, theme::ice, w.onAnim));
        float kx = tx + 11 + (tw - 22) * w.onAnim;
        SolidBrush kb(Lerp(theme::dim, 0xF2F6F9, w.onAnim));
        g.FillEllipse(&kb, kx - 8, ty + th / 2 - 8, 16.0f, 16.0f);
        break;
    }

    case Kind::Selector: {
        draw::FillRound(g, x, y, cw, ch, 9, Lerp(w.bg, w.bgHover, w.hover));
        if (w.open) draw::StrokeRound(g, x + 0.5f, y + 0.5f, cw - 1, ch - 1, 9, Rgb(theme::ice), 1.2f);
        std::wstring t = (w.sel >= 0 && w.sel < (int)w.items.size()) ? w.items[w.sel] : L"";
        draw::Text(g, t, x + 14, y, cw - 48, ch, w.font, false, w.fg, 0);
        float cx = x + cw - 20, cy = y + ch / 2;
        float s = w.open ? -1.0f : 1.0f;
        Line(g, theme::ice, 1.7f, cx - 4.5f, cy - 2 * s, cx, cy + 2.5f * s);
        Line(g, theme::ice, 1.7f, cx, cy + 2.5f * s, cx + 4.5f, cy - 2 * s);
        break;
    }
    }
}

void Panel::DrawSelectorList(Graphics& g, Widget& w) {
    float x = (float)w.r.left, y = (float)w.r.bottom + 4;
    float cw = (float)(w.r.right - w.r.left);
    float lh = 8 + (float)w.items.size() * kItemH;
    draw::FillRound(g, x, y, cw, lh, 9, Rgb(0x14202B));
    draw::StrokeRound(g, x + 0.5f, y + 0.5f, cw - 1, lh - 1, 9, Rgb(theme::line), 1.0f);
    for (int i = 0; i < (int)w.items.size(); i++) {
        float iy = y + 4 + i * kItemH;
        if (i == w.hotItem) draw::FillRound(g, x + 4, iy, cw - 8, (float)kItemH, 6, Rgb(theme::panel2));
        bool sel = (i == w.sel);
        if (sel) {
            SolidBrush dot(Rgb(theme::ice));
            g.FillEllipse(&dot, x + 12, iy + kItemH / 2 - 3.0f, 6.0f, 6.0f);
        }
        draw::Text(g, w.items[i], x + 26, iy, cw - 38, (float)kItemH, 13, false, sel ? theme::ice : theme::silver, 0);
    }
}

}  // namespace ui
