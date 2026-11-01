#include "config.h"
#include "lang.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <shlguid.h>

std::wstring AppDataDir() {
    wchar_t p[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, p);
    std::wstring d = std::wstring(p) + L"\\hollow-mic";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

static std::wstring IniPath() { return AppDataDir() + L"\\settings.ini"; }

static std::wstring ReadStr(const wchar_t* key, const wchar_t* def) {
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(L"main", key, def, buf, 512, IniPath().c_str());
    return buf;
}
static int ReadInt(const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntW(L"main", key, def, IniPath().c_str());
}
static void WriteStr(const wchar_t* key, const std::wstring& v) {
    WritePrivateProfileStringW(L"main", key, v.c_str(), IniPath().c_str());
}
static void WriteInt(const wchar_t* key, int v) { WriteStr(key, std::to_wstring(v)); }

void LoadConfig(Config& c) {
    c.hkMods      = (UINT)ReadInt(L"hotkeyMods", MOD_ALT);
    c.hkVk        = (UINT)ReadInt(L"hotkeyVk", 'C');
    c.mic         = ReadStr(L"mic", L"");
    c.startup     = ReadInt(L"startup", 0) != 0;
    c.startMin    = ReadInt(L"startMin", 0) != 0;
    c.muteOnStart = ReadInt(L"muteOnStart", 0) != 0;
    c.notifyStart = ReadInt(L"notifyStart", 1) != 0;
    c.sounds      = ReadInt(L"sounds", 1) != 0;
    c.trayToggle  = ReadInt(L"trayToggle", 1) != 0;
    if (c.hkVk == 0) { c.hkMods = MOD_ALT; c.hkVk = 'C'; }
}

bool SaveConfig(const Config& c) {
    WriteInt(L"hotkeyMods", (int)c.hkMods);
    WriteInt(L"hotkeyVk", (int)c.hkVk);
    WriteStr(L"mic", c.mic);
    WriteInt(L"startup", c.startup);
    WriteInt(L"startMin", c.startMin);
    WriteInt(L"muteOnStart", c.muteOnStart);
    WriteInt(L"notifyStart", c.notifyStart);
    WriteInt(L"sounds", c.sounds);
    WriteInt(L"trayToggle", c.trayToggle);
    return true;
}

bool SetStartup(bool enable) {
    wchar_t dir[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, dir))) return false;
    std::wstring lnk = std::wstring(dir) + L"\\Hollow Mic.lnk";
    if (!enable) {
        return DeleteFileW(lnk.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring workDir = exe;
    workDir = workDir.substr(0, workDir.find_last_of(L'\\'));

    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(ShellLink), nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(IShellLinkW), (void**)&sl)))
        return false;
    sl->SetPath(exe);
    sl->SetWorkingDirectory(workDir.c_str());
    sl->SetDescription(L"Hollow Mic");
    IPersistFile* pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(sl->QueryInterface(__uuidof(IPersistFile), (void**)&pf))) {
        ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
        pf->Release();
    }
    sl->Release();
    return ok;
}

static std::wstring KeyName(UINT vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::wstring(1, (wchar_t)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    switch (vk) {
        case VK_SPACE: return L"Space";   case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";       case VK_BACK: return L"Backspace";
        case VK_PAUSE: return L"Pause";   case VK_SCROLL: return L"Scroll Lock";
        case VK_SNAPSHOT: return L"Print Screen"; case VK_INSERT: return L"Insert";
        case VK_DELETE: return L"Delete"; case VK_HOME: return L"Home";
        case VK_END: return L"End";       case VK_PRIOR: return L"Page Up";
        case VK_NEXT: return L"Page Down";case VK_LEFT: return L"Left";
        case VK_RIGHT: return L"Right";   case VK_UP: return L"Up";
        case VK_DOWN: return L"Down";
    }
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    wchar_t buf[64] = {};
    if (GetKeyNameTextW((LONG)(sc << 16), buf, 64) > 0) return buf;
    return L"Key " + std::to_wstring(vk);
}

std::wstring HotkeyText(UINT mods, UINT vk) {
    std::wstring s;
    if (mods & MOD_CONTROL) s += L"Ctrl + ";
    if (mods & MOD_ALT)     s += L"Alt + ";
    if (mods & MOD_SHIFT)   s += L"Shift + ";
    return s + KeyName(vk);
}
