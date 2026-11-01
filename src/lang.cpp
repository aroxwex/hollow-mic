#include "lang.h"

static const wchar_t* kStrings[(int)Str::Count] = {
    L"Hollow Mic", L"Mic is On", L"Mic is Off", L"No microphone",
    L"Click to mute", L"Click to unmute", L"Plug in a microphone",
    L"Settings", L"Current hotkey",
    L"Open", L"Toggle Mic", L"Settings", L"Exit",
    L"HOTKEY", L"MICROPHONE", L"STARTUP",
    L"Change", L"Press keys…  (Esc cancels)", L"Add Ctrl, Alt or Shift",
    L"System default microphone", L"All microphones",
    L"Run at Windows startup", L"Start minimized to tray", L"Mute mic on startup",
    L"Notify mic state on start", L"Sounds on mute / unmute",
    L"Tray icon: single click toggles mic",
    L"Defaults", L"Save",
    L"This hotkey is already used by another program.",
    L"Hollow Mic 1.0\nSystem-wide microphone mute.\n\nSettings folder:\n",
};

const wchar_t* T(Str s) { return kStrings[(int)s]; }
