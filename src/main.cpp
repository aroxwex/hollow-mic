// Hollow Mic - system-wide microphone mute toggle.
// Pure Win32 + GDI+. No registry access, settings in %AppData%\hollow-mic\settings.ini
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <memory>
#include <vector>

#include "audio.h"
#include "config.h"
#include "draw.h"
#include "lang.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"

namespace {

enum Ids {
    ID_STATUS = 1, ID_SETTINGS, ID_HELP, ID_MIN, ID_CLOSE, ID_FOOTER,
    ID_HK_BOX, ID_HK_CHANGE, ID_MIC, ID_T_STARTUP, ID_T_STARTMIN, ID_T_MUTESTART,
    ID_T_NOTIFY, ID_T_SOUNDS, ID_T_TRAY, ID_DEFAULTS, ID_SAVE, ID_SET_CLOSE,
    ID_M_OPEN = 100, ID_M_TOGGLE, ID_M_SETTINGS, ID_M_EXIT
};
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_SHOWMAIN = WM_APP + 2;
constexpr UINT WM_DROP_SETTINGS = WM_APP + 3;
constexpr UINT TIMER_SYNC = 1;
constexpr int HOTKEY_ID = 1;
constexpr const wchar_t* kMsgClass = L"HollowMicMsgWnd";

HWND g_msg = nullptr;
UINT g_taskbarMsg = 0;
Config g_cfg, g_pend;
std::unique_ptr<ui::Panel> g_main, g_set, g_menu;
std::vector<MicInfo> g_mics;
bool g_muted = false, g_haveMic = false, g_capturing = false, g_trayAdded = false, g_skipUp = false;
HICON g_icoOn = nullptr, g_icoOff = nullptr;

// ------------------------------------------------------------------ widget helpers
ui::Widget MakeBtn(int id, int x, int y, int w, int h, const wchar_t* text, DWORD bg, DWORD hv,
                   DWORD fg, bool outline = false) {
    ui::Widget b;
    b.id = id; b.kind = ui::Kind::Button; b.r = {x, y, x + w, y + h};
    b.text = text; b.bg = bg; b.bgHover = hv; b.fg = fg; b.font = 14; b.bold = true; b.outline = outline;
    return b;
}
ui::Widget MakeLabel(int id, int x, int y, int w, int h, const wchar_t* text, DWORD fg, float font,
                     bool bold, int align) {
    ui::Widget l;
    l.id = id; l.kind = ui::Kind::Label; l.r = {x, y, x + w, y + h};
    l.text = text; l.fg = fg; l.font = font; l.bold = bold; l.align = align;
    return l;
}
ui::Widget MakeIconBtn(int id, int x, ui::Glyph g, DWORD hover) {
    ui::Widget b;
    b.id = id; b.kind = ui::Kind::IconBtn; b.r = {x, 0, x + 40, ui::kTitleH};
    b.bg = theme::bar; b.bgHover = hover; b.glyph = g;
    return b;
}
ui::Widget MakeToggle(int id, int y, const wchar_t* text) {
    ui::Widget t;
    t.id = id; t.kind = ui::Kind::Toggle; t.r = {24, y, 336, y + 28};
    t.text = text; t.fg = theme::silver; t.font = 14;
    return t;
}
ui::Widget MakeMenuItem(int id, int y, const wchar_t* text) {
    ui::Widget w;
    w.id = id; w.kind = ui::Kind::MenuItem; w.r = {6, y, 214, y + 34};
    w.text = text; w.bg = theme::bg; w.bgHover = theme::panel2; w.fg = theme::silver; w.font = 14;
    return w;
}

// ------------------------------------------------------------------ mic state
void UpdateTray(bool add);

void UpdateUI() {
    if (g_main && g_main->hwnd) {
        if (auto* s = g_main->Find(ID_STATUS)) {
            s->muted = g_haveMic && g_muted;
            s->text = !g_haveMic ? T(Str::NoMic) : g_muted ? T(Str::MicOff) : T(Str::MicOn);
            s->text2 = !g_haveMic ? T(Str::HintNoMic) : g_muted ? T(Str::HintUnmute) : T(Str::HintMute);
        }
        if (auto* f = g_main->Find(ID_FOOTER))
            f->text = std::wstring(T(Str::CurrentHotkey)) + L" :  " + HotkeyText(g_cfg.hkMods, g_cfg.hkVk);
        g_main->Invalidate();
    }
    UpdateTray(false);
}

bool RefreshState(bool force) {
    bool m = false;
    bool ok = GetMute(g_cfg.mic, m);
    if (!force && ok == g_haveMic && (!ok || m == g_muted)) return false;
    g_haveMic = ok;
    g_muted = ok ? m : false;
    UpdateUI();
    return true;
}

void ToggleMic(bool silent = false) {
    bool m = false;
    if (!GetMute(g_cfg.mic, m) || !SetMute(g_cfg.mic, !m)) { RefreshState(true); return; }
    RefreshState(true);
    if (g_cfg.sounds && !silent) PlayMuteSound(g_muted);
}

// ------------------------------------------------------------------ tray
void FillNid(NOTIFYICONDATAW& n) {
    n.cbSize = sizeof(n);
    n.hWnd = g_msg;
    n.uID = 1;
}

void UpdateTray(bool add) {
    if (!g_msg) return;
    NOTIFYICONDATAW n{};
    FillNid(n);
    n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    n.uCallbackMessage = WM_TRAY;
    n.hIcon = (g_haveMic && g_muted) ? g_icoOff : g_icoOn;
    const wchar_t* tip = !g_haveMic ? T(Str::NoMic) : g_muted ? T(Str::MicOff) : T(Str::MicOn);
    lstrcpynW(n.szTip, tip, ARRAYSIZE(n.szTip));
    if (add) { Shell_NotifyIconW(NIM_ADD, &n); g_trayAdded = true; }
    else if (g_trayAdded) Shell_NotifyIconW(NIM_MODIFY, &n);
}

void RemoveTray() {
    if (!g_trayAdded) return;
    NOTIFYICONDATAW n{};
    FillNid(n);
    Shell_NotifyIconW(NIM_DELETE, &n);
    g_trayAdded = false;
}

void Balloon(const wchar_t* text) {
    NOTIFYICONDATAW n{};
    FillNid(n);
    n.uFlags = NIF_INFO;
    lstrcpynW(n.szInfoTitle, T(Str::AppName), ARRAYSIZE(n.szInfoTitle));
    lstrcpynW(n.szInfo, text, ARRAYSIZE(n.szInfo));
    n.dwInfoFlags = NIIF_INFO | (g_cfg.sounds ? 0 : NIIF_NOSOUND);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

// ------------------------------------------------------------------ hotkey
bool RegisterHotkey(UINT mods, UINT vk) {
    UnregisterHotKey(g_msg, HOTKEY_ID);
    return RegisterHotKey(g_msg, HOTKEY_ID, mods | MOD_NOREPEAT, vk) != FALSE;
}

// ------------------------------------------------------------------ windows
void OpenSettings();

void ShowMain() {
    RefreshState(true);
    if (!g_main->Visible()) g_main->MoveBottomRight();
    g_main->Show();
}

void BuildMain() {
    g_main = std::make_unique<ui::Panel>(360, 232, T(Str::AppName));
    g_main->Create();

    g_main->Add(MakeIconBtn(ID_HELP, 240, ui::Glyph::Help, theme::panel));
    g_main->Add(MakeIconBtn(ID_MIN, 280, ui::Glyph::Minimize, theme::panel));
    g_main->Add(MakeIconBtn(ID_CLOSE, 320, ui::Glyph::Close, theme::closeHover));

    ui::Widget st;
    st.id = ID_STATUS; st.kind = ui::Kind::Status; st.r = {24, 56, 336, 126};
    st.bg = theme::panel; st.bgHover = theme::panel2;
    g_main->Add(st);

    g_main->Add(MakeBtn(ID_SETTINGS, 24, 138, 312, 44, T(Str::Settings), theme::panel, theme::panel2,
                        theme::ice));
    g_main->Add(MakeLabel(ID_FOOTER, 24, 194, 312, 22, L"", theme::dim, 12.5f, false, 1));

    g_main->onClick = [](int id) {
        switch (id) {
        case ID_STATUS: ToggleMic(); break;
        case ID_SETTINGS: OpenSettings(); break;
        case ID_HELP: {
            std::wstring msg = std::wstring(T(Str::AboutText)) + AppDataDir();
            MessageBoxW(g_main->hwnd, msg.c_str(), T(Str::AppName), MB_OK | MB_ICONINFORMATION);
            break;
        }
        case ID_MIN: case ID_CLOSE: g_main->Hide(); break;
        }
    };
    g_main->onKey = [](UINT, WPARAM wp, LPARAM) {
        if (wp == VK_ESCAPE) { g_main->Hide(); return true; }
        return false;
    };
}

// ---- dark tray menu (our own popup, no light Windows menu) ------------------
void BuildMenu() {
    g_menu = std::make_unique<ui::Panel>(220, 157, L"", false, true);
    g_menu->Create();
    g_menu->Add(MakeMenuItem(ID_M_OPEN, 6, T(Str::TrayOpen)));
    g_menu->Add(MakeMenuItem(ID_M_TOGGLE, 40, T(Str::TrayToggle)));
    g_menu->Add(MakeMenuItem(ID_M_SETTINGS, 74, T(Str::TraySettings)));
    ui::Widget sep;
    sep.id = 0; sep.kind = ui::Kind::Separator; sep.r = {6, 108, 214, 117};
    g_menu->Add(sep);
    g_menu->Add(MakeMenuItem(ID_M_EXIT, 117, T(Str::TrayExit)));

    g_menu->onClick = [](int id) {
        g_menu->Hide();
        switch (id) {
        case ID_M_OPEN: ShowMain(); break;
        case ID_M_TOGGLE: ToggleMic(); break;
        case ID_M_SETTINGS: OpenSettings(); break;
        case ID_M_EXIT: PostMessageW(g_msg, WM_CLOSE, 0, 0); break;
        }
    };
    g_menu->onDeactivate = [] { g_menu->Hide(); };
    g_menu->onKey = [](UINT, WPARAM wp, LPARAM) {
        if (wp == VK_ESCAPE) { g_menu->Hide(); return true; }
        return false;
    };
}

void ShowTrayMenu() {
    if (!g_menu) BuildMenu();
    if (auto* t = g_menu->Find(ID_M_TOGGLE)) t->text2 = HotkeyText(g_cfg.hkMods, g_cfg.hkVk);
    POINT pt;
    GetCursorPos(&pt);
    RECT wa, rc;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    GetWindowRect(g_menu->hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top, m = 8;
    int x, y;
    if (pt.y >= wa.bottom)      { y = wa.bottom - h - m; x = pt.x - w / 2; }   // taskbar at bottom
    else if (pt.y < wa.top)     { y = wa.top + m;        x = pt.x - w / 2; }   // taskbar at top
    else if (pt.x >= wa.right)  { x = wa.right - w - m;  y = pt.y - h / 2; }   // taskbar at right
    else if (pt.x < wa.left)    { x = wa.left + m;       y = pt.y - h / 2; }   // taskbar at left
    else                        { x = pt.x;              y = pt.y - h; }
    x = std::max<int>(wa.left + m, std::min<int>(x, wa.right - w - m));
    y = std::max<int>(wa.top + m, std::min<int>(y, wa.bottom - h - m));
    g_menu->MoveTo(x, y);
    g_menu->Show();
}

// ---- settings -------------------------------------------------------------
int MicIndexFromCfg(const std::wstring& mic) {
    if (mic.empty()) return 0;
    if (mic == L"*") return 1;
    for (size_t i = 0; i < g_mics.size(); i++)
        if (g_mics[i].id == mic) return (int)i + 2;
    return 0;
}
std::wstring MicFromIndex(int idx) {
    if (idx <= 0) return L"";
    if (idx == 1) return L"*";
    return g_mics[idx - 2].id;
}

void RefreshHotkeyBox() {
    if (!g_set) return;
    if (auto* b = g_set->Find(ID_HK_BOX)) {
        b->text = g_capturing ? T(Str::PressKeys) : HotkeyText(g_pend.hkMods, g_pend.hkVk);
        b->fg = g_capturing ? theme::ice : theme::silver;
    }
    g_set->Invalidate();
}

void BeginCapture() {
    if (g_capturing || !g_set) return;
    g_capturing = true;
    UnregisterHotKey(g_msg, HOTKEY_ID);
    RefreshHotkeyBox();
    SetFocus(g_set->hwnd);
}

void EndCapture() {
    if (!g_capturing) return;
    g_capturing = false;
    RegisterHotkey(g_cfg.hkMods, g_cfg.hkVk);   // restore the active hotkey
    RefreshHotkeyBox();
}

void DropSettings() {
    if (g_capturing) EndCapture();
    PostMessageW(g_msg, WM_DROP_SETTINGS, 0, 0);   // destroy outside of the panel's own callbacks
    if (g_set) g_set->Hide();
}

bool StandaloneKeyOk(UINT vk) {
    return (vk >= VK_F1 && vk <= VK_F24) || vk == VK_PAUSE || vk == VK_SCROLL || vk == VK_SNAPSHOT;
}

bool SettingsKey(UINT, WPARAM wp, LPARAM lp) {
    UINT vk = (UINT)wp;
    if (!g_capturing) {
        if (vk == VK_ESCAPE) {
            auto* sel = g_set->Find(ID_MIC);
            if (sel && sel->open) return false;      // let the panel close the dropdown
            DropSettings();
            return true;
        }
        return false;
    }
    if (lp & (1 << 30)) return true;                 // auto-repeat
    if (vk == VK_ESCAPE) { EndCapture(); return true; }
    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN ||
        vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LSHIFT || vk == VK_RSHIFT ||
        vk == VK_LMENU || vk == VK_RMENU)
        return true;
    UINT mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
    if (!mods && !StandaloneKeyOk(vk)) {
        if (auto* b = g_set->Find(ID_HK_BOX)) b->text = T(Str::NeedMod);
        g_set->Invalidate();
        return true;
    }
    g_pend.hkMods = mods;
    g_pend.hkVk = vk;
    EndCapture();
    return true;
}

void SyncSettingsWidgets() {
    auto setOn = [](int id, bool v) { if (auto* t = g_set->Find(id)) t->on = v; };
    setOn(ID_T_STARTUP, g_pend.startup);
    setOn(ID_T_STARTMIN, g_pend.startMin);
    setOn(ID_T_MUTESTART, g_pend.muteOnStart);
    setOn(ID_T_NOTIFY, g_pend.notifyStart);
    setOn(ID_T_SOUNDS, g_pend.sounds);
    setOn(ID_T_TRAY, g_pend.trayToggle);
    if (auto* s = g_set->Find(ID_MIC)) s->sel = MicIndexFromCfg(g_pend.mic);
    RefreshHotkeyBox();
    g_set->Refresh();
}

void SaveSettings() {
    if (g_capturing) return;
    if (!RegisterHotkey(g_pend.hkMods, g_pend.hkVk)) {
        MessageBoxW(g_set->hwnd, T(Str::ErrHotkeyUsed), T(Str::AppName), MB_OK | MB_ICONWARNING);
        RegisterHotkey(g_cfg.hkMods, g_cfg.hkVk);
        return;
    }
    g_cfg = g_pend;
    SaveConfig(g_cfg);
    SetStartup(g_cfg.startup);
    RefreshState(true);
    DropSettings();
}

void OpenSettings() {
    if (g_set) {
        if (g_set->Visible()) { g_set->Show(); return; }
        g_set.reset();                               // hidden leftover from a closed window
    }
    g_pend = g_cfg;
    g_mics = ListMics();

    g_set = std::make_unique<ui::Panel>(360, 488, T(Str::Settings));
    g_set->Create();

    g_set->Add(MakeIconBtn(ID_SET_CLOSE, 320, ui::Glyph::Close, theme::closeHover));

    g_set->Add(MakeLabel(0, 24, 50, 312, 18, T(Str::SecHotkey), theme::ice, 11.5f, true, 0));
    ui::Widget box;
    box.id = ID_HK_BOX; box.kind = ui::Kind::Box; box.r = {24, 72, 246, 108};
    box.bg = theme::panel; box.bgHover = theme::panel2; box.fg = theme::silver; box.font = 14;
    box.text = HotkeyText(g_pend.hkMods, g_pend.hkVk);
    g_set->Add(box);
    g_set->Add(MakeBtn(ID_HK_CHANGE, 254, 72, 82, 36, T(Str::Change), theme::panel, theme::panel2, theme::ice));

    g_set->Add(MakeLabel(0, 24, 122, 312, 18, T(Str::SecMic), theme::ice, 11.5f, true, 0));
    ui::Widget sel;
    sel.id = ID_MIC; sel.kind = ui::Kind::Selector; sel.r = {24, 144, 336, 180};
    sel.bg = theme::panel; sel.bgHover = theme::panel2; sel.fg = theme::silver; sel.font = 14;
    sel.items.push_back(T(Str::SysDefault));
    sel.items.push_back(T(Str::AllMics));
    for (auto& m : g_mics) sel.items.push_back(m.name);
    g_set->Add(sel);

    g_set->Add(MakeLabel(0, 24, 194, 312, 18, T(Str::SecStartup), theme::ice, 11.5f, true, 0));
    g_set->Add(MakeToggle(ID_T_STARTUP, 218, T(Str::TStartup)));
    g_set->Add(MakeToggle(ID_T_STARTMIN, 250, T(Str::TStartMin)));
    g_set->Add(MakeToggle(ID_T_MUTESTART, 282, T(Str::TMuteStart)));
    g_set->Add(MakeToggle(ID_T_NOTIFY, 314, T(Str::TNotify)));
    g_set->Add(MakeToggle(ID_T_SOUNDS, 346, T(Str::TSounds)));
    g_set->Add(MakeToggle(ID_T_TRAY, 378, T(Str::TTrayClick)));

    g_set->Add(MakeBtn(ID_DEFAULTS, 24, 426, 150, 42, T(Str::Defaults), theme::panel, theme::panel2,
                       theme::silver, true));
    g_set->Add(MakeBtn(ID_SAVE, 186, 426, 150, 42, T(Str::Save), theme::ice, theme::silver, theme::bg));

    SyncSettingsWidgets();
    g_set->SnapAnimations();

    g_set->onClick = [](int id) {
        auto on = [&](int tid) { return g_set->Find(tid)->on; };
        switch (id) {
        case ID_T_STARTUP: g_pend.startup = on(id); break;
        case ID_T_STARTMIN: g_pend.startMin = on(id); break;
        case ID_T_MUTESTART: g_pend.muteOnStart = on(id); break;
        case ID_T_NOTIFY: g_pend.notifyStart = on(id); break;
        case ID_T_SOUNDS: g_pend.sounds = on(id); break;
        case ID_T_TRAY: g_pend.trayToggle = on(id); break;
        case ID_HK_BOX: case ID_HK_CHANGE: BeginCapture(); break;
        case ID_DEFAULTS: {
            if (g_capturing) EndCapture();
            Config d;
            g_pend = d;
            SyncSettingsWidgets();
            break;
        }
        case ID_SAVE: SaveSettings(); break;
        case ID_SET_CLOSE: DropSettings(); break;
        }
    };
    g_set->onSelect = [](int, int idx) { g_pend.mic = MicFromIndex(idx); };
    g_set->onKey = SettingsKey;
    g_set->onCloseRequest = DropSettings;

    g_set->MoveCenter();
    g_set->Show();
}

// ------------------------------------------------------------------ message window
LRESULT CALLBACK MsgProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == g_taskbarMsg && g_taskbarMsg) { g_trayAdded = false; UpdateTray(true); return 0; }
    switch (m) {
    case WM_HOTKEY: ToggleMic(); return 0;
    case WM_TIMER: if (wp == TIMER_SYNC) RefreshState(false); return 0;
    case WM_TRAY:
        switch (LOWORD(lp)) {
        case WM_LBUTTONUP:
            if (g_skipUp) { g_skipUp = false; break; }
            if (g_cfg.trayToggle) ToggleMic(); else ShowMain();
            break;
        case WM_LBUTTONDBLCLK:
            if (g_cfg.trayToggle) { ToggleMic(true); g_skipUp = true; }   // undo first click silently
            ShowMain();
            break;
        case WM_RBUTTONUP: ShowTrayMenu(); break;
        }
        return 0;
    case WM_SHOWMAIN: ShowMain(); return 0;
    case WM_DROP_SETTINGS: g_set.reset(); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\HollowMicSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(kMsgClass, nullptr);
        if (other) PostMessageW(other, WM_SHOWMAIN, 0, 0);
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    draw::Init();
    ui::InitScale();
    LoadConfig(g_cfg);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kMsgClass;
    RegisterClassExW(&wc);
    g_msg = CreateWindowExW(0, kMsgClass, L"Hollow Mic", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    g_taskbarMsg = RegisterWindowMessageW(L"TaskbarCreated");

    int ico = GetSystemMetrics(SM_CXSMICON);
    g_icoOn = draw::MakeMicIcon(ico, theme::ice, false);
    g_icoOff = draw::MakeMicIcon(ico, theme::red, true);

    BuildMain();

    if (!RegisterHotkey(g_cfg.hkMods, g_cfg.hkVk)) {
        Config d;
        g_cfg.hkMods = d.hkMods;
        g_cfg.hkVk = d.hkVk;
        if (!RegisterHotkey(g_cfg.hkMods, g_cfg.hkVk))
            MessageBoxW(nullptr, T(Str::ErrHotkeyUsed), T(Str::AppName), MB_OK | MB_ICONWARNING);
    }
    if (g_cfg.muteOnStart) SetMute(g_cfg.mic, true);
    RefreshState(true);
    UpdateTray(true);
    if (g_cfg.startup) SetStartup(true);          // self-heal the Startup-folder shortcut
    if (g_cfg.notifyStart) Balloon(!g_haveMic ? T(Str::NoMic) : g_muted ? T(Str::MicOff) : T(Str::MicOn));
    SetTimer(g_msg, TIMER_SYNC, 3000, nullptr);
    if (!g_cfg.startMin) ShowMain();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(g_msg, HOTKEY_ID);
    RemoveTray();
    g_menu.reset();
    g_set.reset();
    g_main.reset();
    if (g_icoOn) DestroyIcon(g_icoOn);
    if (g_icoOff) DestroyIcon(g_icoOff);
    draw::Shutdown();
    CoUninitialize();
    CloseHandle(mutex);
    return 0;
}
