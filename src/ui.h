#pragma once
#include "draw.h"
#include <functional>
#include <string>
#include <vector>

namespace ui {

constexpr int kTitleH = 36;

enum class Kind { Label, Button, IconBtn, Toggle, Box, Selector, Status, MenuItem, Separator };
enum class Glyph { None, Help, Minimize, Close };

struct Widget {
    int id = 0;
    Kind kind = Kind::Label;
    RECT r{0, 0, 0, 0};                 // logical (96-dpi) units
    std::wstring text, text2;
    DWORD bg = 0, bgHover = 0, fg = 0;
    float font = 13;
    bool bold = false;
    int align = 0;                      // 0 left, 1 center, 2 right
    Glyph glyph = Glyph::None;
    bool outline = false;
    bool on = false;                    // Toggle state
    bool muted = false;                 // Status state
    std::vector<std::wstring> items;    // Selector
    int sel = 0;
    bool open = false;
    int hotItem = -1;
    bool visible = true;
    // animation
    float hover = 0, hoverTarget = 0, onAnim = 0;
};

class Panel {
public:
    Panel(int w, int h, std::wstring title, bool titleBar = true, bool topmost = false);
    ~Panel();

    bool Create();
    void Destroy();
    void Show();
    void Hide();
    bool Visible() const;
    void MoveBottomRight();
    void MoveCenter();
    void MoveTo(int x, int y);
    void Invalidate();

    Widget& Add(const Widget& w);
    Widget* Find(int id);
    void SnapAnimations();              // jump animations to their targets
    void Refresh() { StartAnim(); Invalidate(); }   // restart animations + repaint

    HWND hwnd = nullptr;
    std::function<void(int)> onClick;           // button / toggle clicked
    std::function<void(int, int)> onSelect;     // selector item chosen (id, index)
    std::function<bool(UINT, WPARAM, LPARAM)> onKey;
    std::function<void()> onCloseRequest;
    std::function<void()> onDeactivate;         // window lost activation (used by the tray menu)

private:
    struct Hit { int w = -1; int item = -1; };

    static LRESULT CALLBACK StaticProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT Proc(UINT, WPARAM, LPARAM);
    void Paint(HDC hdc);
    void DrawWidget(Gdiplus::Graphics& g, Widget& w);
    void DrawSelectorList(Gdiplus::Graphics& g, Widget& w);
    Hit HitTest(float x, float y);
    void StartAnim();
    void Animate();
    void CloseSelectors();
    void ApplyCorners();

    int w_, h_;
    bool titleBar_, topmost_;
    std::wstring title_;
    std::vector<Widget> ws_;
    float scale_ = 1.0f;
    bool tracking_ = false;
    int pressed_ = -1;
    bool timer_ = false;
};

extern float g_scale;
void InitScale();

}  // namespace ui
