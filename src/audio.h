#pragma once
#include <string>
#include <vector>

struct MicInfo {
    std::wstring id;
    std::wstring name;
};

// Must be called on a thread that has called CoInitializeEx.
std::vector<MicInfo> ListMics();
// id: "" = system default (+ default communications device), "*" = all active microphones,
// otherwise an endpoint id. GetMute reports muted only if every targeted device is muted.
// Both return false if no usable device exists.
bool GetMute(const std::wstring& id, bool& muted);
bool SetMute(const std::wstring& id, bool muted);
