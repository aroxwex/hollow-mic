#include "audio.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <propidl.h>

template <class T>
struct Com {
    T* p = nullptr;
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    ~Com() { if (p) p->Release(); }
    void** out() { return (void**)&p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

static const PROPERTYKEY kFriendlyName = {
    {0xa45c254e, 0xdf1c, 0x4efc, {0x80, 0x14, 0xf2, 0xd4, 0xe8, 0xdc, 0x7c, 0xb5}}, 14};

static bool CreateEnumerator(Com<IMMDeviceEnumerator>& en) {
    return SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), en.out()));
}

// A set of capture endpoints to act on together.
struct VolSet {
    std::vector<IAudioEndpointVolume*> vols;
    std::vector<std::wstring> ids;
    ~VolSet() { for (auto* v : vols) v->Release(); }
};

static void AddDevice(IMMDevice* dev, VolSet& set) {
    std::wstring id;
    LPWSTR pid = nullptr;
    if (SUCCEEDED(dev->GetId(&pid)) && pid) { id = pid; CoTaskMemFree(pid); }
    for (auto& e : set.ids) if (e == id) return;               // already included
    IAudioEndpointVolume* v = nullptr;
    if (SUCCEEDED(dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&v)) && v) {
        set.vols.push_back(v);
        set.ids.push_back(id);
    }
}

// sel: ""  -> system default capture device + default communications device
//      "*" -> every active capture device
//      id  -> that device (falls back to the default if it is gone)
static bool Collect(const std::wstring& sel, VolSet& set) {
    Com<IMMDeviceEnumerator> en;
    if (!CreateEnumerator(en)) return false;
    if (sel == L"*") {
        Com<IMMDeviceCollection> coll;
        if (SUCCEEDED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &coll.p))) {
            UINT n = 0;
            coll->GetCount(&n);
            for (UINT i = 0; i < n; i++) {
                Com<IMMDevice> d;
                if (SUCCEEDED(coll->Item(i, &d.p))) AddDevice(d.p, set);
            }
        }
        return !set.vols.empty();
    }
    if (!sel.empty()) {
        Com<IMMDevice> d;
        if (SUCCEEDED(en->GetDevice(sel.c_str(), &d.p))) AddDevice(d.p, set);
    }
    if (set.vols.empty()) {
        const ERole roles[2] = {eConsole, eCommunications};
        for (ERole r : roles) {
            Com<IMMDevice> d;
            if (SUCCEEDED(en->GetDefaultAudioEndpoint(eCapture, r, &d.p))) AddDevice(d.p, set);
        }
    }
    return !set.vols.empty();
}

bool GetMute(const std::wstring& id, bool& muted) {
    VolSet set;
    if (!Collect(id, set)) return false;
    bool all = true;
    for (auto* v : set.vols) {
        BOOL m = FALSE;
        if (FAILED(v->GetMute(&m))) return false;
        if (!m) all = false;
    }
    muted = all;      // "muted" only when every targeted device is muted
    return true;
}

bool SetMute(const std::wstring& id, bool muted) {
    VolSet set;
    if (!Collect(id, set)) return false;
    bool ok = true;
    for (auto* v : set.vols)
        if (FAILED(v->SetMute(muted ? TRUE : FALSE, nullptr))) ok = false;
    return ok;
}

std::vector<MicInfo> ListMics() {
    std::vector<MicInfo> out;
    Com<IMMDeviceEnumerator> en;
    if (!CreateEnumerator(en)) return out;
    Com<IMMDeviceCollection> coll;
    if (FAILED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &coll.p))) return out;
    UINT n = 0;
    coll->GetCount(&n);
    for (UINT i = 0; i < n; i++) {
        Com<IMMDevice> dev;
        if (FAILED(coll->Item(i, &dev.p))) continue;
        MicInfo mi;
        LPWSTR pid = nullptr;
        if (SUCCEEDED(dev->GetId(&pid)) && pid) {
            mi.id = pid;
            CoTaskMemFree(pid);
        }
        Com<IPropertyStore> store;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &store.p))) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(store->GetValue(kFriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal)
                mi.name = pv.pwszVal;
            PropVariantClear(&pv);
        }
        if (mi.name.empty()) mi.name = L"Microphone";
        if (!mi.id.empty()) out.push_back(mi);
    }
    return out;
}
