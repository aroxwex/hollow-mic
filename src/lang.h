#pragma once

enum class Str {
    AppName, MicOn, MicOff, NoMic, HintMute, HintUnmute, HintNoMic,
    Settings, CurrentHotkey,
    TrayOpen, TrayToggle, TraySettings, TrayExit,
    SecHotkey, SecMic, SecStartup,
    Change, PressKeys, NeedMod, SysDefault, AllMics,
    TStartup, TStartMin, TMuteStart, TNotify, TSounds, TTrayClick,
    Defaults, Save, ErrHotkeyUsed, AboutText,
    Count
};

const wchar_t* T(Str s);   // all UI strings live in lang.cpp (English only)
