#include "sound.h"
#include <windows.h>
#include <mmsystem.h>

namespace {
constexpr int kResOn = 101;    // hollow_on.wav  (see res/app.rc)
constexpr int kResOff = 102;   // hollow_off.wav

const void* LoadWav(int id) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return nullptr;
    HGLOBAL mem = LoadResource(nullptr, res);
    return mem ? LockResource(mem) : nullptr;   // resource memory stays valid for the process lifetime
}
}  // namespace

void PlayMuteSound(bool muted) {
    const void* wav = LoadWav(muted ? kResOff : kResOn);
    if (wav) PlaySoundW(static_cast<LPCWSTR>(wav), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
