#pragma once
#include <windows.h>
#include <string>

struct Config {
    UINT hkMods = MOD_ALT;          // MOD_* flags
    UINT hkVk   = 'C';
    std::wstring mic;               // endpoint id; empty = system default (+communications); "*" = all microphones
    bool startup     = false;
    bool startMin    = false;
    bool muteOnStart = false;
    bool notifyStart = true;
    bool sounds      = true;
    bool trayToggle  = true;        // single click on tray icon toggles the mic
};

std::wstring AppDataDir();                    // %AppData%\hollow-mic (created if missing)
void LoadConfig(Config& c);
bool SaveConfig(const Config& c);
bool SetStartup(bool enable);                 // Startup-folder shortcut, no registry
std::wstring HotkeyText(UINT mods, UINT vk);  // "Alt + Z"
