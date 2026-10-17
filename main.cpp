// BudsPlus: native Win32 control app for Nothing and CMF earbuds (a desktop port of ear (web)).
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#include "bt.hpp"
#include "models.hpp"
#include "settings.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <ctime>
#include <functional>
#include <unordered_map>

constexpr UINT WM_LINK = WM_APP + 1, WM_TRAY = WM_APP + 2, WM_TESTQUERY = WM_APP + 100;
constexpr UINT_PTR TimerInit = 1, TimerReconnect = 2, TimerBattery = 3, TimerSave = 4, TimerFit = 5, TimerScroll = 6;
const wchar_t* const AppName = L"BudsPlus";
const wchar_t* const MainClass = L"BudsPlus.Main";

enum Cmd : int {
    CmdConnect = 100, CmdDisconnect, CmdRefresh, CmdDeviceMenu, CmdMoreMenu, CmdShowAll, CmdBluetoothSettings, CmdAutoConnect,
    CmdTray, CmdCloseToTray, CmdStartMinimized, CmdLowBattery, CmdSettingsFolder, CmdAbout, CmdExit, CmdShow, CmdCancel,
    CmdDeviceBase = 1000,                 // + index into devices
    CmdModelBase = 2000,                  // + index into models::AllCaps; CmdModelBase - 1 = automatic
    CmdThemeBase = 3100,                  // + theme
    CmdAncBase = 5000,                    // + ANC wire value
    CmdEqBase = 5100,                     // + listening mode
    CmdBassToggle = 5200, CmdBassLevelBase = 5210,   // + UI level (1-5, or 1-2 for three-stage models)
    CmdSpatialBase = 5300,                // + index into SpatialModes
    CmdInEar = 5400, CmdLatency, CmdPersonal, CmdDual,
    CmdCodecBase = 5500,                  // + codec index
    CmdFitTest = 5600, CmdRingLeft = 5610, CmdRingRight, CmdRingBoth,
    CmdGestureSideBase = 5700,            // + side
    CmdGestureSlotBase = 5710,            // + slot: opens the action menu
    CmdNoiseCycleBase = 5750,             // + 0 transparency, 1 noise cancellation, 2 off (tap-and-hold cycle)
    CmdGestureBase = 6000,                // + side * 400 + slot * 100 + option index
    CmdCustomEqBase = 7000,               // + band * 100 + (dB + 6)
    CmdDualDeviceBase = 8000,             // + index into the dual-connection list
};

// ---------------------------------------------------------------- appearance

// Windows 11 (Fluent) colours, the same family TDM uses: neutral greys and the blue accent.
struct Palette {
    bool dark;
    COLORREF bg,            // window background
        surface, border,    // cards
        text, muted,
        hover,              // row hover inside a card
        control, controlHover, controlBorder,   // buttons, the raised segment, slider thumbs
        well,               // the recessed track behind segmented controls
        rail,               // slider and progress rails, the off toggle
        accent, accentHover, accentText, success, warning, danger;
};
const Palette LightPalette{false, RGB(243, 243, 243), RGB(251, 251, 251), RGB(229, 229, 229), RGB(27, 27, 27), RGB(96, 96, 96), RGB(240, 240, 240),
                           RGB(255, 255, 255), RGB(246, 246, 246), RGB(214, 214, 214), RGB(237, 237, 237), RGB(138, 138, 138),
                           RGB(0, 95, 184), RGB(25, 113, 194), RGB(255, 255, 255), RGB(15, 123, 15), RGB(157, 93, 0), RGB(196, 43, 28)};
const Palette DarkPalette{true, RGB(32, 32, 32), RGB(43, 43, 43), RGB(29, 29, 29), RGB(240, 240, 240), RGB(160, 160, 160), RGB(50, 50, 50),
                          RGB(55, 55, 55), RGB(61, 61, 61), RGB(68, 68, 68), RGB(36, 36, 36), RGB(158, 158, 158),
                          RGB(76, 194, 255), RGB(71, 177, 232), RGB(0, 0, 0), RGB(108, 203, 95), RGB(252, 225, 0), RGB(255, 120, 110)};
const Palette AmoledPalette{true, RGB(0, 0, 0), RGB(18, 18, 18), RGB(36, 36, 36), RGB(230, 230, 230), RGB(140, 140, 140), RGB(28, 28, 28),
                            RGB(30, 30, 30), RGB(38, 38, 38), RGB(52, 52, 52), RGB(8, 8, 8), RGB(130, 130, 130),
                            RGB(76, 194, 255), RGB(71, 177, 232), RGB(0, 0, 0), RGB(108, 203, 95), RGB(252, 225, 0), RGB(255, 120, 110)};

// Undocumented uxtheme exports (Windows 10 1903+) for dark context menus.
namespace darkmode {
using SetPreferredAppModeFn = int(WINAPI*)(int);
using FlushMenuThemesFn = void(WINAPI*)();
SetPreferredAppModeFn setPreferredAppMode = nullptr;
FlushMenuThemesFn flushMenuThemes = nullptr;
void init() {
    using RtlGetNtVersionNumbersFn = void(WINAPI*)(DWORD*, DWORD*, DWORD*);
    auto rtl = (RtlGetNtVersionNumbersFn)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetNtVersionNumbers");
    DWORD major = 0, minor = 0, build = 0;
    if (rtl) rtl(&major, &minor, &build);
    build &= ~0xF0000000u;
    if (major < 10 || build < 18362) return;
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux) return;
    setPreferredAppMode = (SetPreferredAppModeFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    flushMenuThemes = (FlushMenuThemesFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(136));
}
void app(bool dark) { if (setPreferredAppMode) setPreferredAppMode(dark ? 2 : 3); if (flushMenuThemes) flushMenuThemes(); }
} // namespace darkmode

HINSTANCE gInst;
HWND gMain;
settings::Settings S;
std::wstring gSettingsPath;
bool gTest = false;                     // BUDSPLUS_TEST: confirmations answer yes, no balloons
Palette P = LightPalette;
int dpi = 96;
int px(int v) { return MulDiv(v, dpi, 96); }
HFONT uiFont, uiBold, uiTitle, uiLarge, uiSmall, iconFont, iconSmall;
std::wstring iconFace = L"Segoe Fluent Icons", textFace = L"Segoe UI";

void fill(HDC dc, const RECT& r, COLORREF c) { SetDCBrushColor(dc, c); FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH)); }

// GDI draws curves without anti-aliasing, which leaves stair-stepped edges on rounded shapes. paint() renders into
// a 32-bit DIB, so rounded rectangles and circles are filled here with per-pixel coverage on their corners instead.
struct Canvas { uint32_t* bits = nullptr; int w = 0, h = 0; RECT clip{}; } gCanvas;
void blendPixel(int x, int y, COLORREF c, double cover) {
    if (x < gCanvas.clip.left || x >= gCanvas.clip.right || y < gCanvas.clip.top || y >= gCanvas.clip.bottom || cover <= 0) return;
    uint32_t& d = gCanvas.bits[y * gCanvas.w + x];
    int a = (int)(std::min(cover, 1.0) * 256);
    auto mix = [&](int dst, int src) { return dst + (((src - dst) * a) >> 8); };
    d = (uint32_t)(mix((d >> 16) & 255, GetRValue(c)) << 16 | mix((d >> 8) & 255, GetGValue(c)) << 8 | mix(d & 255, GetBValue(c)));
}
// Fills r with corners of the given radius (in pixels; r.bottom - r.top for a pill or a circle's full height).
void fillRounded(HDC dc, RECT r, COLORREF c, double radius) {
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    radius = std::max(0.0, std::min(radius, std::min(w, h) / 2.0));
    if (!gCanvas.bits || radius < 1) { fill(dc, r, c); return; }
    GdiFlush();
    int k = (int)std::ceil(radius);
    // Straight parts through GDI: the middle band, and the strips between the corners.
    RECT mid{r.left, r.top + k, r.right, r.bottom - k}, top{r.left + k, r.top, r.right - k, r.top + k}, bot{r.left + k, r.bottom - k, r.right - k, r.bottom};
    RECT clipped;
    for (RECT* p : {&mid, &top, &bot}) if (IntersectRect(&clipped, p, &gCanvas.clip)) fill(dc, clipped, c);
    GdiFlush();
    // Corners: the share of 4x4 samples per pixel that lie within `radius` of the inner rectangle.
    double x0 = r.left + radius, x1 = r.right - radius, y0 = r.top + radius, y1 = r.bottom - radius;
    auto corner = [&](int v, int lo, int hi) { return v < lo + k || v >= hi - k; };
    for (int y = r.top; y < r.bottom; ++y) {
        if (!corner(y, r.top, r.bottom)) continue;
        for (int x = r.left; x < r.right; ++x) {
            if (!corner(x, r.left, r.right)) continue;
            int in = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    double sxp = x + (sx + 0.5) / 4, syp = y + (sy + 0.5) / 4;
                    double dx = sxp - std::max(x0, std::min(sxp, x1)), dy = syp - std::max(y0, std::min(syp, y1));
                    if (dx * dx + dy * dy <= radius * radius) ++in;
                }
            blendPixel(x, y, c, in / 16.0);
        }
    }
}
// A rounded rectangle with a 1px border (border == face for none).
void roundRect(HDC dc, const RECT& r, COLORREF face, COLORREF border, double radius) {
    if (border == face) { fillRounded(dc, r, face, radius); return; }
    fillRounded(dc, r, border, radius);
    fillRounded(dc, RECT{r.left + 1, r.top + 1, r.right - 1, r.bottom - 1}, face, std::max(0.0, radius - 1));
}
void circle(HDC dc, int cx, int cy, int d, COLORREF c) { fillRounded(dc, RECT{cx - d / 2, cy - d / 2, cx - d / 2 + d, cy - d / 2 + d}, c, d / 2.0); }
void drawText(HDC dc, const std::wstring& s, RECT r, COLORREF c, HFONT f, UINT fmt = DT_LEFT) {
    SelectObject(dc, f); SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, fmt | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}
int textWidth(HDC dc, HFONT f, const std::wstring& s) { SelectObject(dc, f); SIZE sz{}; GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz); return sz.cx; }
bool hasGlyph(wchar_t c) {
    static std::unordered_map<wchar_t, bool> known;
    auto it = known.find(c);
    if (it != known.end()) return it->second;
    HDC dc = GetDC(nullptr); HGDIOBJ o = SelectObject(dc, iconFont);
    WORD g = 0xFFFF; GetGlyphIndicesW(dc, &c, 1, &g, GGI_MARK_NONEXISTING_GLYPHS);
    SelectObject(dc, o); ReleaseDC(nullptr, dc);
    return known[c] = g != 0xFFFF;
}
void drawGlyph(HDC dc, const std::wstring& g, RECT r, COLORREF c, HFONT f = nullptr) {
    if (g.empty() || !hasGlyph(g[0])) return;
    drawText(dc, g, r, c, f ? f : iconFont, DT_CENTER);
}
COLORREF blend(COLORREF a, COLORREF b, int pct) {
    auto m = [&](int x, int y) { return (x * (100 - pct) + y * pct) / 100; };
    return RGB(m(GetRValue(a), GetRValue(b)), m(GetGValue(a), GetGValue(b)), m(GetBValue(a), GetBValue(b)));
}

// Segoe Fluent Icons / MDL2 code points.
const wchar_t* const GlyphBluetooth = L"";
const wchar_t* const GlyphMore = L"";
const wchar_t* const GlyphChevron = L"";
const wchar_t* const GlyphBolt = L"";
const wchar_t* const GlyphHeadphone = L"";
const wchar_t* const GlyphRefresh = L"";

// ---------------------------------------------------------------- session

enum Conn { Idle, Connecting, Connected };
bt::Link gLink;
Conn conn = Idle;
bt::Target target;
std::wstring targetName;
std::wstring statusMessage;           // the last error, shown while idle
std::string modelId;                  // Fast Pair model ID of the connected device
const models::Caps* caps = nullptr;
const models::FastPair* fastPairInfo = nullptr;
bool modelGuessed = false;            // caps came from the device name, not from Fast Pair
proto::State st;
uint8_t opId = 0;
bool userDisconnected = false, awaitingReboot = false;
int reconnectAttempts = 0;
bool ringing[2] = {false, false};
bool fitRunning = false;
int dualPages = 0;
bool lowWarned[3] = {false, false, false};
std::deque<std::pair<uint16_t, proto::Bytes>> initQueue;
std::vector<bt::Device> devices;
bool showAllDevices = false;
bool radioOn = true;
const char* gFakeSpec = nullptr;

void invalidate() { if (gMain) InvalidateRect(gMain, nullptr, FALSE); }
void updateTray();
void scheduleSave() { if (gMain) SetTimer(gMain, TimerSave, 400, nullptr); }

// BUDSPLUS_LOG=<file>: append every packet sent and received, in hex, for protocol debugging.
FILE* gLog = nullptr;
void logPacket(const char* dir, const proto::Bytes& b) {
    if (!gLog) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(gLog, "%02d:%02d:%02d.%03d %s", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, dir);
    for (uint8_t c : b) fprintf(gLog, " %02X", c);
    fputc('\n', gLog);
    fflush(gLog);
}

void send(uint16_t command, const proto::Bytes& payload = {}) {
    if (conn != Connected) return;
    if (++opId > 250) opId = 1;
    proto::Bytes b = proto::build(command, payload, opId);
    logPacket(">", b);
    gLink.send(b);
}
bool supported(bool flag) { return caps && flag; }
bool isEarbuds() { return !caps || caps->kind == models::Earbuds; }
bool threeStage() { return caps && caps->threeStageBass; }

std::wstring deviceTitle() {
    if (conn == Idle && targetName.empty()) return AppName;
    if (fastPairInfo) return fastPairInfo->name;
    if (caps) return caps->name;
    return targetName.empty() ? std::wstring(AppName) : targetName;
}

void refreshDevices() {
    if (gFakeSpec) {
        devices.clear();
        bt::Device d; d.addr = 0; d.name = L"CMF Buds 2 Plus (test)"; d.connected = true;
        devices.push_back(d);
        return;
    }
    radioOn = bt::radioOn();
    devices = bt::pairedDevices();
    std::stable_sort(devices.begin(), devices.end(), [](const bt::Device& a, const bt::Device& b) {
        bool na = models::looksLikeNothing(a.name), nb = models::looksLikeNothing(b.name);
        if (na != nb) return na;
        return a.connected && !b.connected;
    });
}

void queueInit() {
    initQueue.clear();
    auto q = [](uint16_t c, proto::Bytes p = {}) { initQueue.emplace_back(c, std::move(p)); };
    q(proto::GetBattery);
    q(proto::GetFirmware);
    uint32_t now = (uint32_t)time(nullptr);
    if (!caps || std::string(caps->base) != "B181") q(proto::SetTime, {(uint8_t)(now >> 24), (uint8_t)(now >> 16), (uint8_t)(now >> 8), (uint8_t)now});
    if (!caps) return;
    if (caps->ancLevel >= 0) q(proto::GetAnc);
    if (caps->listeningMode) { q(proto::GetListeningMode); q(proto::GetCustomEq); }
    else q(proto::GetEq);
    if (caps->inEar) q(proto::GetInEar);
    q(proto::GetLatency);
    if (caps->espeonGestures) q(proto::GetGesture);
    q(proto::GetAdvancedEq);
    if (caps->bass) q(proto::GetBass);
    if (caps->dual) { q(proto::GetDualEnable); q(proto::GetDualList, {0}); }
    if (caps->codecs) q(proto::GetCodec);
    if (caps->spatial) q(proto::GetSpatial);
    if (caps->personal == 1) q(proto::GetMimi);
    if (caps->personal == 2) q(proto::GetAudiodo);
}

void resetDeviceState() {
    st = proto::State{};
    ringing[0] = ringing[1] = false;
    fitRunning = false;
    dualPages = 0;
    initQueue.clear();
}

void connectTo(const bt::Target& t, const std::wstring& name) {
    KillTimer(gMain, TimerReconnect);
    target = t;
    targetName = name;
    statusMessage.clear();
    modelId.clear();
    caps = nullptr;
    fastPairInfo = nullptr;
    modelGuessed = false;
    resetDeviceState();
    userDisconnected = false;
    conn = Connecting;
    gLink.start(t);
    if (!t.fake()) { S.device = bt::addrString(t.addr); scheduleSave(); }
    SetWindowTextW(gMain, (deviceTitle() + L" - " + AppName).c_str());
    invalidate();
    updateTray();
}
void connectDevice(const bt::Device& d) {
    bt::Target t;
    if (gFakeSpec) bt::parseFake(gFakeSpec, t);
    else t.addr = d.addr;
    connectTo(t, d.name);
}
void disconnect(bool byUser) {
    userDisconnected = byUser;
    gLink.stop();
    conn = Idle;
    KillTimer(gMain, TimerInit);
    KillTimer(gMain, TimerBattery);
    if (byUser) KillTimer(gMain, TimerReconnect);
    invalidate();
    updateTray();
}
void scheduleReconnect(int seconds) {
    if (!S.autoConnect && !awaitingReboot) return;
    SetTimer(gMain, TimerReconnect, (UINT)seconds * 1000, nullptr);
}

void resolveModel(const std::string& id) {
    std::wstring key = target.fake() ? L"fake" : bt::addrString(target.addr);
    modelId = id;
    if (modelId.empty()) {
        auto it = S.models.find(key);
        if (it != S.models.end()) modelId = settings::toUtf8(it->second);
    } else {
        S.models[key] = settings::fromUtf8(modelId);
        scheduleSave();
    }
    fastPairInfo = models::fastPair(modelId);
    if (fastPairInfo) caps = models::capsFor(fastPairInfo->base);
    else if (modelId.size() == 4 && modelId[0] == 'B') caps = models::capsFor(modelId);   // a model the user chose by hand
    if (!caps) { caps = models::guessFromName(targetName); modelGuessed = caps != nullptr; }
}

void lowBatteryCheck();

void onLinkEvents() {
    for (auto& e : gLink.drain()) {
        if (e.generation != gLink.current()) continue;
        switch (e.type) {
        case bt::EvModel:
            resolveModel(e.model);
            SetWindowTextW(gMain, (deviceTitle() + L" - " + AppName).c_str());
            break;
        case bt::EvConnected:
            conn = Connected;
            reconnectAttempts = 0;
            awaitingReboot = false;
            queueInit();
            SetTimer(gMain, TimerInit, 90, nullptr);
            SetTimer(gMain, TimerBattery, 60000, nullptr);
            break;
        case bt::EvPacket: {
            logPacket("<", e.packet.raw);
            unsigned ch = proto::apply(st, e.packet);
            if (ch & proto::ChDual) {
                if (st.dualListGrew && (int)st.dual.size() < st.dualTotal && ++dualPages < 20) send(proto::GetDualList, {(uint8_t)(st.dual.size() & 0xFF)});
                if (st.dualListStale) { st.dualListStale = false; st.dual.clear(); dualPages = 0; send(proto::GetDualList, {0}); }
            }
            if (ch & proto::ChFit) { fitRunning = false; KillTimer(gMain, TimerFit); }
            if (ch & proto::ChBattery) lowBatteryCheck();
            break;
        }
        case bt::EvClosed:
        case bt::EvFailed: {
            bool wasConnected = conn == Connected;
            conn = Idle;
            KillTimer(gMain, TimerInit);
            KillTimer(gMain, TimerBattery);
            statusMessage = e.error;
            if (awaitingReboot) {
                statusMessage = L"Rebooting the earbuds...";
                if (++reconnectAttempts > 7) { awaitingReboot = false; statusMessage = L"The earbuds did not come back after the reboot."; }
                else scheduleReconnect(wasConnected ? 9 : 5);
            } else if (!userDisconnected) {
                ++reconnectAttempts;
                scheduleReconnect(std::min(60, 5 * reconnectAttempts));
            }
            break;
        }
        }
    }
    invalidate();
    updateTray();
}

// ---------------------------------------------------------------- actions

bool confirm(const std::wstring& title, const std::wstring& text) {
    if (gTest) return true;
    return MessageBoxW(gMain, text.c_str(), title.c_str(), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK;
}
void info(const std::wstring& title, const std::wstring& text) {
    if (gTest) return;
    MessageBoxW(gMain, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

void setAnc(int wire) {
    if (!caps || caps->ancLevel < 0) return;
    st.anc = wire;
    send(proto::SetAnc, {0x01, (uint8_t)wire, 0x00});
}
// Feature conflicts ear (web) enforces: on Dirac tier-1 models Dirac OPTEO and LDAC exclude each other;
// on "mutuallyExclusive" models (B170) spatial audio excludes bass enhance.
bool ldacBlocksDirac() { return caps && caps->dirac == 1 && st.codec == 2; }
bool spatialExcludesBass() { return caps && std::string(caps->base) == "B170"; }

void setEqPreset(int mode) {
    if (!caps || !caps->listeningMode) return;
    if (mode == 0 && ldacBlocksDirac()) { info(L"Equalizer", L"Dirac OPTEO is not available while LDAC is on. Switch Audio quality to AAC first."); return; }
    st.eqMode = mode;
    send(proto::SetListeningMode, {(uint8_t)mode, 0x00});
    if (mode == models::CustomPreset) send(proto::GetCustomEq);
}
void setCustomEq(int band, int value) {
    if (!caps || !caps->listeningMode) return;
    st.custom[band] = std::max(-6, std::min(6, value));
    st.haveCustom = true;
    if (st.eqMode != models::CustomPreset) { st.eqMode = models::CustomPreset; send(proto::SetListeningMode, {(uint8_t)models::CustomPreset, 0x00}); }
    send(proto::SetCustomEq, proto::customEqPayload(st.custom[0], st.custom[1], st.custom[2], caps->eq));
}
int bassUiLevel() {
    if (threeStage()) return st.bassWire >= 10 ? 2 : 1;
    return std::max(1, std::min(5, st.bassWire / 2));
}
int bassLevels() { return threeStage() ? 2 : 5; }
void sendBass() {
    int level = std::max(1, std::min(bassLevels(), bassUiLevel()));
    if (st.bassWire <= 0) st.bassWire = threeStage() ? 5 : 2;
    int wire = threeStage() ? (level == 2 ? 10 : 5) : level * 2;
    send(proto::SetBass, {(uint8_t)(st.bassOn == 1 ? 1 : 0), (uint8_t)wire});
}
void setBassLevel(int level) {
    if (st.bassOn != 1 && spatialExcludesBass() && st.spatialMode > 0) { info(L"Bass Enhance", L"Bass Enhance is not available while spatial audio is on."); return; }
    level = std::max(1, std::min(bassLevels(), level));
    st.bassWire = threeStage() ? (level == 2 ? 10 : 5) : level * 2;
    st.bassOn = 1;
    sendBass();
}
void ring(int side) {
    // side 0 left, 1 right, 2 single-unit devices
    if (side == 2) {
        ringing[0] = !ringing[0];
        if (caps && std::string(caps->base) == "B181") send(proto::Ring, {(uint8_t)ringing[0]});
        else send(proto::Ring, {0x06, (uint8_t)ringing[0]});
        return;
    }
    ringing[side] = !ringing[side];
    send(proto::Ring, {(uint8_t)(side == 0 ? 0x02 : 0x03), (uint8_t)ringing[side]});
}
int gestureAction(int side, int type) {
    for (auto& g : st.gestures) if (g.device == (side == 0 ? 2 : 3) && g.type == type) return g.action;
    return -1;
}
void setGesture(int side, int type, int action) {
    uint8_t device = side == 0 ? 2 : 3;
    bool found = false;
    for (auto& g : st.gestures) if (g.device == device && g.type == type) { g.action = (uint8_t)action; found = true; }
    if (!found) st.gestures.push_back(proto::Gesture{device, 1, (uint8_t)type, (uint8_t)action});
    send(proto::SetGesture, {0x01, device, 0x01, (uint8_t)type, (uint8_t)action});
}
void toggleNoiseCycle(int part) {
    // The tap-and-hold noise-control cycle is shared by both earbuds; keep at least two modes in it.
    int side = S.gestureSide;
    int a = gestureAction(side, 7);
    if (!models::isNoiseControlAction(a)) a = 10;
    bool v[3];
    models::noiseControlParts(a, v[0], v[1], v[2]);
    v[part] = !v[part];
    int next = models::noiseControlAction(v[0], v[1], v[2]);
    if (!next) return;
    setGesture(0, 7, next);
    setGesture(1, 7, next);
}
void setPersonal(bool on) {
    if (!caps || !caps->personal) return;
    if (on && caps->personal == 1 && st.codec == 2) { info(L"Personal sound", L"Personal sound is not available while LDAC is on."); return; }
    st.personal = on;
    send(caps->personal == 1 ? proto::SetMimi : proto::SetAudiodo, {(uint8_t)on});
}
void setDualEnabled(bool on) {
    if (!caps || !caps->dual) return;
    if (caps->dualReboot && !confirm(L"Reboot required", L"Turning dual connection on or off restarts the earbuds. They reconnect by themselves in a few seconds."))
        return;
    st.dualEnabled = on;
    send(proto::SetDualEnable, {(uint8_t)on});
    if (caps->dualReboot) { awaitingReboot = true; reconnectAttempts = 0; }
    else if (on) { st.dual.clear(); dualPages = 0; send(proto::GetDualList, {0}); }
}
void setCodec(int idx) {
    if (!caps || idx == st.codec) return;
    std::wstring warn = idx == 2 ? L"LDAC can shorten battery life, and interference may cause choppy audio. " : L"";
    if (idx == 2 && caps->dirac == 1) warn += L"Dirac OPTEO is not available with LDAC. ";
    if (!confirm(L"Reboot required", warn + L"Changing the audio codec restarts the earbuds. They reconnect by themselves in a few seconds.")) return;
    st.codec = idx;
    awaitingReboot = true;
    reconnectAttempts = 0;
    send(proto::SetCodec, {(uint8_t)idx});
}
void setSpatial(int idx) {
    if (!caps || !caps->spatial) return;
    if (idx != 0 && spatialExcludesBass() && (st.bassOn == 1 || st.advancedEq == 1)) {
        info(L"Spatial audio", st.advancedEq == 1 ? L"Spatial audio is not available while Advanced EQ is on." : L"Spatial audio is not available while Bass Enhance is on.");
        return;
    }
    auto& m = models::SpatialModes[idx];
    st.spatialMode = m.mode;
    st.spatialHead = m.head;
    send(proto::SetSpatial, {(uint8_t)m.mode, (uint8_t)m.head});
}
void startFitTest() {
    if (!caps || !caps->fitTest) return;
    fitRunning = true;
    st.fitLeft = st.fitRight = -1;
    send(proto::EarFitTest, {0x01});
    SetTimer(gMain, TimerFit, 20000, nullptr);
}
void toggleDualDevice(int i) {
    if (i < 0 || i >= (int)st.dual.size()) return;
    auto& d = st.dual[i];
    if (d.self) return;
    uint8_t mac[6];
    if (!proto::macBytes(d.mac, mac)) return;
    bool connect = !d.connected;
    proto::Bytes p{(uint8_t)connect};
    p.insert(p.end(), mac, mac + 6);
    send(proto::SetDualConnect, p);
}

std::wstring fitText() {
    if (fitRunning) return L"Testing... keep both earbuds in and stay quiet";
    if (st.fitLeft < 0) return L"Checks the seal of each ear tip";
    if (st.fitLeft == 2 || st.fitRight == 2) return L"Put both earbuds in your ears, then run the test";
    if (st.fitLeft == 0 && st.fitRight == 0) return L"Good seal on both sides";
    if (st.fitLeft == 1 && st.fitRight == 1) return L"Adjust both earbuds or try another tip size";
    if (st.fitLeft == 1) return L"Adjust the left earbud or try another tip size";
    return L"Adjust the right earbud or try another tip size";
}

void lowBatteryCheck() {
    if (!S.notifyLowBattery) return;
    const proto::Battery* b[3] = {&st.left, &st.right, &st.stereo};
    const wchar_t* names[3] = {L"Left earbud", L"Right earbud", L"Battery"};
    for (int i = 0; i < 3; ++i) {
        if (b[i]->level < 0 || b[i]->charging || b[i]->level > 20) { lowWarned[i] = false; continue; }
        if (b[i]->level <= 15 && !lowWarned[i]) {
            lowWarned[i] = true;
            extern void balloon(const std::wstring&, const std::wstring&);
            balloon(deviceTitle() + L": low battery", std::wstring(names[i]) + L" is at " + std::to_wstring(b[i]->level) + L"%.");
        }
    }
}

// ---------------------------------------------------------------- tray

NOTIFYICONDATAW nid{};
bool trayAdded = false, reallyQuit = false;
UINT WM_TASKBARCREATED = 0;

std::wstring batteryLine() {
    auto part = [](const wchar_t* label, const proto::Battery& b) {
        if (b.level < 0) return std::wstring();
        return std::wstring(label) + std::to_wstring(b.level) + L"%" + (b.charging ? L"+" : L"");
    };
    std::vector<std::wstring> parts;
    if (st.stereo.level >= 0) parts.push_back(part(L"", st.stereo));
    else {
        for (auto& s : {part(L"L ", st.left), part(L"R ", st.right), part(L"Case ", st.caseB)}) if (!s.empty()) parts.push_back(s);
    }
    std::wstring out;
    for (auto& s : parts) { if (!out.empty()) out += L"  "; out += s; }
    return out;
}
void updateTray() {
    if (!trayAdded) return;
    std::wstring tip = conn == Connected ? deviceTitle() : conn == Connecting ? L"Connecting to " + deviceTitle() : std::wstring(AppName) + L": not connected";
    std::wstring b = conn == Connected ? batteryLine() : L"";
    if (!b.empty()) tip += L"\n" + b;
    nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy(nid.szTip, tip.c_str(), 127);
    nid.szTip[127] = 0;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}
void setTray(bool on) {
    if (on == trayAdded) return;
    if (on) {
        nid = NOTIFYICONDATAW{};
        nid.cbSize = sizeof nid; nid.hWnd = gMain; nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP; nid.uCallbackMessage = WM_TRAY;
        nid.hIcon = (HICON)LoadImageW(gInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        if (!nid.hIcon) nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wcsncpy(nid.szTip, AppName, 127);
        trayAdded = Shell_NotifyIconW(NIM_ADD, &nid) != 0;
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
        updateTray();
    } else {
        Shell_NotifyIconW(NIM_DELETE, &nid);
        trayAdded = false;
    }
}
void balloon(const std::wstring& title, const std::wstring& text) {
    if (gTest || !trayAdded) return;
    nid.uFlags = NIF_INFO;
    wcsncpy(nid.szInfoTitle, title.c_str(), 63); nid.szInfoTitle[63] = 0;
    wcsncpy(nid.szInfo, text.c_str(), 255); nid.szInfo[255] = 0;
    nid.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}
void showMain() {
    if (!IsWindowVisible(gMain)) ShowWindow(gMain, SW_SHOW);
    if (IsIconic(gMain)) ShowWindow(gMain, SW_RESTORE);
    SetForegroundWindow(gMain);
}

// ---------------------------------------------------------------- layout and painting
//
// The window is painted in one pass: render() lays out the header and the cards, draws them and records
// the clickable rectangles in `hits`. Cards are measured first (Ctx::draw false) and then drawn.

struct Hit { RECT r; int cmd; int kind; int arg; };    // kind 0 button, 1 EQ slider (arg = band)
std::vector<Hit> hits;
int hoverHit = -1, pressHit = -1;
int scrollY = 0, scrollTarget = 0, contentH = 0, bodyTop = 0;   // scrollY eases towards scrollTarget
int dragBand = -1, dragValue = 0;
RECT sliderTrack[3]{};
bool scrollDrag = false, scrollHot = false;
int scrollDragStart = 0, scrollDragOrigin = 0;
RECT scrollThumb{};

struct Ctx { HDC dc; bool draw; int off; RECT body; };
Ctx C;

RECT rc(int x, int y, int w, int h) { return RECT{x, y + C.off, x + w, y + h + C.off}; }
void addHit(const RECT& r, int cmd, int kind = 0, int arg = 0) {
    if (!C.draw) return;
    RECT i;
    if (IntersectRect(&i, &r, &C.body)) hits.push_back(Hit{i, cmd, kind, arg});
}
bool hovered(int cmd) { return hoverHit >= 0 && hoverHit < (int)hits.size() && hits[hoverHit].cmd == cmd; }
bool pressed(int cmd) { return pressHit >= 0 && pressHit < (int)hits.size() && hits[pressHit].cmd == cmd; }

void text(int x, int y, int w, int h, const std::wstring& s, COLORREF c, HFONT f, UINT fmt = DT_LEFT) { if (C.draw) drawText(C.dc, s, rc(x, y, w, h), c, f, fmt); }

void button(const RECT& r, const std::wstring& label, int cmd, bool primary = false, bool enabled = true) {
    if (!C.draw) return;
    bool hot = enabled && hovered(cmd), down = hot && pressed(cmd);
    COLORREF face = primary ? (hot ? P.accentHover : P.accent) : (hot ? P.controlHover : P.control);
    COLORREF border = primary ? face : P.controlBorder, fg = primary ? P.accentText : P.text;
    if (down) fg = blend(fg, face, 30);
    if (!enabled) { fg = P.muted; face = P.control; border = P.controlBorder; }
    roundRect(C.dc, r, face, border, px(4));
    drawText(C.dc, label, r, fg, primary ? uiBold : uiFont, DT_CENTER);
    if (enabled) addHit(r, cmd);
}

// A row of mutually exclusive options: a recessed well with the chosen one raised and underlined in the accent,
// like the selector bars in Windows 11.
struct Seg { std::wstring label; int cmd; bool selected; bool enabled; };
int segmented(int x, int y, int w, const std::vector<Seg>& segs) {
    int h = px(38);
    if (C.draw && !segs.empty()) {
        RECT all = rc(x, y, w, h);
        roundRect(C.dc, all, P.well, P.dark ? P.border : P.controlBorder, px(6));
        int n = (int)segs.size();
        for (int i = 0; i < n; ++i) {
            int a = x + w * i / n, b = x + w * (i + 1) / n;
            RECT r = rc(a + px(3), y + px(3), b - a - px(6), h - px(6));
            const Seg& s = segs[i];
            bool hot = s.enabled && hovered(s.cmd);
            if (s.selected) {
                roundRect(C.dc, r, s.enabled ? P.control : P.well, P.controlBorder, px(4));
                if (s.enabled) {
                    int iw = px(16), ih = px(3);
                    fillRounded(C.dc, RECT{(r.left + r.right - iw) / 2, r.bottom - ih - px(1), (r.left + r.right + iw) / 2, r.bottom - px(1)}, P.accent, ih / 2.0);
                }
            } else if (hot) fillRounded(C.dc, r, P.dark ? P.hover : P.controlHover, px(4));
            RECT tr = r; tr.bottom -= px(5);
            drawText(C.dc, s.label, tr, !s.enabled ? P.muted : s.selected || hot ? P.text : P.muted, s.selected ? uiBold : uiFont, DT_CENTER);
            if (s.enabled) addHit(r, s.cmd);
        }
    }
    return h;
}
// A grid of toggle buttons: the chosen one is filled with the accent, as a checked Windows 11 toggle button is.
int chips(int x, int y, int w, const std::vector<Seg>& segs, int cols) {
    int h = px(32), gap = px(8), rows = ((int)segs.size() + cols - 1) / cols;
    if (C.draw) {
        for (int i = 0; i < (int)segs.size(); ++i) {
            int c = i % cols, r = i / cols;
            int cw = (w - gap * (cols - 1)) / cols;
            RECT b = rc(x + c * (cw + gap), y + r * (h + gap), cw, h);
            const Seg& s = segs[i];
            bool hot = hovered(s.cmd);
            COLORREF face = s.selected ? (hot ? P.accentHover : P.accent) : hot ? P.controlHover : P.control;
            roundRect(C.dc, b, face, s.selected ? face : P.controlBorder, px(4));
            drawText(C.dc, s.label, b, s.selected ? P.accentText : P.text, s.selected ? uiBold : uiFont, DT_CENTER);
            addHit(b, s.cmd);
        }
    }
    return rows * h + (rows - 1) * gap;
}
// The Windows 11 toggle switch: 40 x 20, an outlined track with a small knob when off, accent-filled when on.
void toggleSwitch(const RECT& r, bool on, bool enabled, bool hot) {
    int h = r.bottom - r.top;
    if (on && enabled) fillRounded(C.dc, r, hot ? P.accentHover : P.accent, h / 2.0);
    else roundRect(C.dc, r, hot && enabled ? P.controlHover : P.surface, enabled ? P.rail : P.controlBorder, h / 2.0);
    int k = px(hot && enabled ? 14 : 12);
    int cx = on ? r.right - h / 2 : r.left + h / 2;
    circle(C.dc, cx, (r.top + r.bottom) / 2, k, on && enabled ? P.accentText : enabled ? P.rail : P.controlBorder);
}
int toggleRow(int x, int y, int w, const std::wstring& label, const std::wstring& sub, int state, int cmd) {
    int h = sub.empty() ? px(44) : px(56);
    if (C.draw) {
        RECT row = rc(x - px(8), y, w + px(16), h);
        bool enabled = state >= 0, hot = enabled && hovered(cmd);
        if (hot) fillRounded(C.dc, row, P.hover, px(4));
        int tw = w - px(60);
        if (sub.empty()) text(x, y, tw, h, label, enabled ? P.text : P.muted, uiFont);
        else {
            text(x, y + px(8), tw, px(20), label, enabled ? P.text : P.muted, uiFont);
            text(x, y + px(28), tw, px(18), sub, P.muted, uiSmall);
        }
        toggleSwitch(rc(x + w - px(40), y + (h - px(20)) / 2, px(40), px(20)), state == 1, enabled, hot);
        if (enabled) addHit(row, cmd);
    }
    return h;
}
int chevronRow(int x, int y, int w, const std::wstring& label, const std::wstring& value, int cmd) {
    int h = px(44);
    if (C.draw) {
        RECT row = rc(x - px(8), y, w + px(16), h);
        if (hovered(cmd)) fillRounded(C.dc, row, P.hover, px(4));
        text(x, y, w / 2, h, label, P.text, uiFont);
        text(x + w / 2, y, w / 2 - px(24), h, value, P.muted, uiFont, DT_RIGHT);
        if (C.draw) drawGlyph(C.dc, GlyphChevron, rc(x + w - px(18), y, px(18), h), P.muted, iconSmall);
        addHit(row, cmd);
    }
    return h;
}
int sliderRow(int x, int y, int w, const wchar_t* label, int band, int value) {
    int h = px(40);
    if (dragBand == band) value = dragValue;
    if (C.draw) {
        int lw = px(64), vw = px(56);
        text(x, y, lw, h, label, P.text, uiFont);
        wchar_t v[16]; swprintf(v, 16, value > 0 ? L"+%d dB" : L"%d dB", value);
        text(x + w - vw, y, vw, h, v, P.muted, uiFont, DT_RIGHT);
        int tx = x + lw + px(8), tw = w - lw - vw - px(16);
        RECT track = rc(tx, y + h / 2 - px(2), tw, px(4));
        fillRounded(C.dc, track, P.rail, px(2));
        int mid = tx + tw / 2, pos = tx + (value + 6) * tw / 12;
        RECT tick = rc(mid - px(1) / 2, y + h / 2 - px(6), std::max(1, px(1)), px(12));
        fill(C.dc, tick, P.rail);
        RECT filled = value >= 0 ? rc(mid, y + h / 2 - px(2), pos - mid, px(4)) : rc(pos, y + h / 2 - px(2), mid - pos, px(4));
        if (value) fillRounded(C.dc, filled, P.accent, px(2));
        // The Windows 11 slider thumb: a raised disc with an accent dot that grows on hover.
        bool hot = dragBand == band || hovered(CmdCustomEqBase + band * 100);
        int cy = y + C.off + h / 2;
        circle(C.dc, pos, cy, px(20) + 2, P.controlBorder);
        circle(C.dc, pos, cy, px(20), P.control);
        circle(C.dc, pos, cy, px(dragBand == band ? 10 : hot ? 14 : 12), P.accent);
        sliderTrack[band] = RECT{tx, track.top, tx + tw, track.bottom};
        addHit(rc(tx - px(10), y, tw + px(20), h), CmdCustomEqBase + band * 100, 1, band);
    }
    return h;
}
int noteText(int x, int y, int w, const std::wstring& s, COLORREF c, HFONT f) {
    int h = px(20);
    if (C.draw) {
        RECT r = rc(x, y, w, h);
        SelectObject(C.dc, f); SetTextColor(C.dc, c); SetBkMode(C.dc, TRANSPARENT);
        RECT calc = r;
        DrawTextW(C.dc, s.c_str(), (int)s.size(), &calc, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        r.bottom = r.top + std::max<LONG>(h, calc.bottom - calc.top);
        DrawTextW(C.dc, s.c_str(), (int)s.size(), &r, DT_WORDBREAK | DT_NOPREFIX);
        h = r.bottom - r.top;
    } else {
        HDC dc = C.dc;
        SelectObject(dc, f);
        RECT calc{0, 0, w, h};
        DrawTextW(dc, s.c_str(), (int)s.size(), &calc, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        h = std::max<int>(h, calc.bottom - calc.top);
    }
    return h;
}

// Cards follow the Windows 11 Settings layout: a semibold group title above an 8px-rounded card.
const int CardPad = 16, CardGap = 16;
int cardX = 0, cardW = 0;     // inner content box of the current card column
template <class F> int card(int y, const std::wstring& title, F body) {
    int pad = px(CardPad), titleH = title.empty() ? 0 : px(30);
    bool d = C.draw;
    C.draw = false;
    int h = body(y + titleH + pad);
    C.draw = d;
    int boxH = pad + h + pad;
    if (C.draw) {
        RECT r = rc(cardX - pad, y + titleH, cardW + 2 * pad, boxH);
        if (r.bottom >= C.body.top && r.top - titleH <= C.body.bottom) {
            if (!title.empty()) text(cardX - pad + px(2), y, cardW, px(22), title, P.text, uiBold);
            roundRect(C.dc, r, P.surface, P.border, px(8));
            body(y + titleH + pad);
        }
    }
    return titleH + boxH + px(CardGap);
}

std::wstring upper(std::wstring s) { for (auto& c : s) c = (wchar_t)towupper(c); return s; }

int batteryTile(int x, int y, int w, const wchar_t* label, const proto::Battery& b, int ringCmd, bool isRinging) {
    int h = px(ringCmd ? 108 : 76);
    if (!C.draw) return h;
    bool have = b.level >= 0;
    text(x, y, w, px(20), label, P.muted, uiSmall, DT_CENTER);
    std::wstring pct = have ? std::to_wstring(b.level) + L"%" : L"--";
    int tw = textWidth(C.dc, uiLarge, pct);
    int gx = x + (w - tw) / 2;
    text(x, y + px(18), w, px(32), pct, have ? P.text : P.muted, uiLarge, DT_CENTER);
    if (have && b.charging) drawGlyph(C.dc, GlyphBolt, rc(gx + tw + px(2), y + px(18), px(18), px(32)), P.success, iconSmall);
    // A Windows 11 progress bar: a thin rail with a thicker rounded fill.
    int bw = std::min(w - px(16), px(96));
    RECT bar = rc(x + (w - bw) / 2, y + px(57), bw, px(4));
    RECT rail = bar; rail.top += px(1); rail.bottom = rail.top + std::max(1, px(1));
    fill(C.dc, rail, P.rail);
    if (have && b.level > 0) {
        RECT f = bar;
        f.right = f.left + std::max<LONG>(px(4), bw * b.level / 100);
        COLORREF c = b.charging ? P.success : b.level <= 15 ? P.danger : P.accent;
        fillRounded(C.dc, f, c, px(2));
    }
    if (ringCmd) {
        int rw = px(80);
        RECT rb = rc(x + (w - rw) / 2, y + px(74), rw, px(30));
        button(rb, isRinging ? L"Stop" : L"Ring", ringCmd, isRinging, have || isRinging);
    }
    return h;
}

int body(int y) {
    int x = cardX, w = cardW;
    // ---- not connected: device list
    if (conn != Connected) {
        if (conn == Connecting) {
            y += card(y, L"", [&](int yy) {
                int h = 0;
                text(x, yy, w, px(24), L"Connecting to " + (targetName.empty() ? deviceTitle() : targetName) + L"...", P.text, uiBold);
                h += px(26);
                h += noteText(x, yy + h, w, L"Take the earbuds out of the case. The first connection can take up to 20 seconds.", P.muted, uiSmall);
                h += px(10);
                button(rc(x, yy + h, px(110), px(32)), L"Cancel", CmdCancel);
                return h + px(32);
            });
        } else if (statusMessage.empty() && (target.addr || target.fakePort)) {
            y += card(y, L"", [&](int yy) {
                std::wstring name = targetName.empty() ? deviceTitle() : targetName;
                text(x, yy, w - px(130), px(32), name, P.text, uiBold);
                button(rc(x + w - px(120), yy, px(120), px(32)), L"Connect", CmdConnect, true);
                return px(32);
            });
        } else if (!statusMessage.empty()) {
            y += card(y, L"", [&](int yy) {
                int h = noteText(x, yy, w, statusMessage, awaitingReboot ? P.text : P.accent, uiFont);
                if (!awaitingReboot && target.addr | target.fakePort) {
                    h += px(10);
                    button(rc(x, yy + h, px(110), px(32)), L"Try again", CmdConnect, true);
                    h += px(32);
                }
                return h;
            });
        }
        y += card(y, L"Paired devices", [&](int yy) {
            int h = 0, shown = 0;
            for (int i = 0; i < (int)devices.size(); ++i) {
                auto& d = devices[i];
                if (!showAllDevices && !models::looksLikeNothing(d.name)) continue;
                ++shown;
                int rh = px(50);
                if (C.draw) {
                    RECT row = rc(x - px(8), yy + h, w + px(16), rh);
                    if (hovered(CmdDeviceBase + i)) fillRounded(C.dc, row, P.hover, px(4));
                    drawGlyph(C.dc, GlyphHeadphone, rc(x, yy + h, px(24), rh), P.text);
                    text(x + px(34), yy + h + px(6), w - px(130), px(20), d.name.empty() ? bt::addrString(d.addr) : d.name, P.text, uiFont);
                    text(x + px(34), yy + h + px(26), w - px(130), px(18), d.connected ? L"Connected to this PC" : L"Paired", P.muted, uiSmall);
                    bool busy = conn == Connecting && target.addr == d.addr && !target.fake();
                    button(rc(x + w - px(92), yy + h + px(10), px(92), px(30)), busy ? L"Connecting" : L"Connect", CmdDeviceBase + i, false, !busy);
                }
                h += rh;
            }
            if (!shown) {
                h += noteText(x, yy + h, w, !radioOn ? L"Bluetooth is off, or this PC has no Bluetooth adapter. Turn Bluetooth on in Windows Settings, then press F5." : devices.empty() ? L"No paired Bluetooth devices. Pair your earbuds in Windows Settings first (open the case and hold the button until the light flashes)."
                                                       : L"No Nothing or CMF devices are paired with this PC.", P.muted, uiFont);
            }
            h += px(10);
            button(rc(x, yy + h, px(170), px(32)), L"Bluetooth settings", CmdBluetoothSettings);
            button(rc(x + px(180), yy + h, px(150), px(32)), showAllDevices ? L"Only Nothing / CMF" : L"Show all devices", CmdShowAll);
            return h + px(32);
        });
        return y;
    }

    // ---- connected
    if (!caps || modelGuessed) {
        y += card(y, L"", [&](int yy) {
            std::wstring msg = !caps ? L"This device did not report a model BudsPlus knows. Choose it under More > Model to unlock its controls."
                                     : L"Model detected from the device name (" + std::wstring(caps->name) + L"). If controls look wrong, pick the model under More > Model.";
            return noteText(x, yy, w, msg, P.muted, uiSmall);
        });
    }

    y += card(y, L"Battery", [&](int yy) {
        bool single = st.stereo.level >= 0 || !isEarbuds();
        if (single) {
            int tw = w / 2;
            return batteryTile(x + (w - tw) / 2, yy, tw, L"Battery", st.stereo.level >= 0 ? st.stereo : st.left, CmdRingBoth, ringing[0]);
        }
        int tw = w / 3;
        int h = batteryTile(x, yy, tw, L"Left", st.left, CmdRingLeft, ringing[0]);
        batteryTile(x + tw, yy, tw, L"Case", st.caseB, 0, false);
        batteryTile(x + 2 * tw, yy, w - 2 * tw, L"Right", st.right, CmdRingRight, ringing[1]);
        return h;
    });

    if (caps && caps->ancLevel >= 0) {
        y += card(y, L"Noise control", [&](int yy) {
            int lvl = caps->ancLevel;
            bool nc = proto::ancIsNoiseCancelling(st.anc);
            std::vector<Seg> modes = {{L"Noise cancelling", CmdAncBase + (nc ? st.anc : (lvl & 0x04 ? proto::AncHigh : proto::AncHigh)), nc, true}};
            if (lvl & 0x10) modes.push_back({L"Transparency", CmdAncBase + proto::AncTransparency, st.anc == proto::AncTransparency, true});
            modes.push_back({L"Off", CmdAncBase + proto::AncOff, st.anc == proto::AncOff, true});
            int h = segmented(x, yy, w, modes);
            if (lvl & 0x04) {
                std::vector<Seg> strength = {{L"High", CmdAncBase + proto::AncHigh, st.anc == proto::AncHigh, nc}};
                if (lvl & 0x02) strength.push_back({L"Mid", CmdAncBase + proto::AncMid, st.anc == proto::AncMid, nc});
                strength.push_back({L"Low", CmdAncBase + proto::AncLow, st.anc == proto::AncLow, nc});
                if (lvl & 0x08) strength.push_back({L"Adaptive", CmdAncBase + proto::AncAdaptive, st.anc == proto::AncAdaptive, nc});
                h += px(14);
                text(x, yy + h, w, px(20), L"Strength", nc ? P.text : P.muted, uiSmall);
                h += px(22);
                h += segmented(x, yy + h, w, strength);
            }
            return h;
        });
    }

    if (caps && caps->listeningMode) {
        y += card(y, L"Equalizer", [&](int yy) {
            std::vector<Seg> presets;
            for (auto& o : models::ListeningPresets) {
                if (o.wire == 0 && !caps->dirac) continue;
                presets.push_back({o.label, CmdEqBase + o.wire, st.eqMode == o.wire, true});
            }
            int h = chips(x, yy, w, presets, w >= px(380) ? 3 : 2);
            if (st.eqMode == models::CustomPreset) {
                h += px(12);
                h += sliderRow(x, yy + h, w, L"Bass", 0, st.custom[0]);
                h += sliderRow(x, yy + h, w, L"Mid", 1, st.custom[1]);
                h += sliderRow(x, yy + h, w, L"Treble", 2, st.custom[2]);
            }
            if (st.advancedEq == 1) { h += px(8); h += noteText(x, yy + h, w, L"Advanced EQ is on (set in the Nothing X app); presets switch it off.", P.muted, uiSmall); }
            return h;
        });
    }

    if (caps && caps->bass) {
        y += card(y, caps->listeningMode ? L"Ultra Bass" : L"Bass enhance", [&](int yy) {
            int h = toggleRow(x, yy, w, L"Ultra Bass", L"", st.bassOn < 0 ? 0 : st.bassOn, CmdBassToggle);
            if (st.bassOn == 1) {
                std::vector<Seg> levels;
                for (int i = 1; i <= bassLevels(); ++i) levels.push_back({L"Level " + std::to_wstring(i), CmdBassLevelBase + i, bassUiLevel() == i, true});
                h += px(6);
                h += segmented(x, yy + h, w, levels);
            }
            return h;
        });
    }

    if (caps && caps->spatial) {
        y += card(y, L"Spatial audio", [&](int yy) {
            int cur = st.spatialMode < 0 ? -1 : models::spatialIndex(st.spatialMode, st.spatialHead);
            std::vector<Seg> modes;
            for (int i = 0; i < 6; ++i)
                if (i == 0 || (caps->spatial & models::SpatialModes[i].bit)) modes.push_back({models::SpatialModes[i].label, CmdSpatialBase + i, cur == i, true});
            if (modes.size() <= 4) return segmented(x, yy, w, modes);
            return chips(x, yy, w, modes, 3);
        });
    }

    if (caps && caps->espeonGestures) {
        y += card(y, L"Controls", [&](int yy) {
            int h = segmented(x, yy, w, {{L"Left earbud", CmdGestureSideBase + 0, S.gestureSide == 0, true}, {L"Right earbud", CmdGestureSideBase + 1, S.gestureSide == 1, true}});
            h += px(8);
            auto& slots = models::espeonSlots();
            for (int i = 0; i < (int)slots.size(); ++i) {
                int a = gestureAction(S.gestureSide, slots[i].type);
                std::wstring value = a < 0 ? L"--" : L"Custom";
                for (auto& o : slots[i].actions) if (o.wire == a || (o.wire == 10 && models::isNoiseControlAction(a))) value = o.label;
                h += chevronRow(x, yy + h, w, slots[i].label, value, CmdGestureSlotBase + i);
            }
            return h;
        });
    }

    y += card(y, L"Settings", [&](int yy) {
        int h = 0;
        if (supported(caps && caps->inEar)) h += toggleRow(x, yy + h, w, L"In-ear detection", L"Pause when an earbud comes out", st.inEar, CmdInEar);
        h += toggleRow(x, yy + h, w, L"Low latency mode", L"For games and video", st.latency, CmdLatency);
        if (supported(caps && caps->personal)) h += toggleRow(x, yy + h, w, L"Personal sound", L"Use the hearing profile made in the Nothing X app", st.personal, CmdPersonal);
        if (supported(caps && caps->dual)) h += toggleRow(x, yy + h, w, L"Dual connection", L"Stay connected to two devices", st.dualEnabled, CmdDual);
        if (supported(caps && caps->codecs)) {
            h += px(6);
            text(x, yy + h, w, px(20), L"Audio quality", P.text, uiFont);
            h += px(24);
            std::vector<Seg> codecs = {{L"AAC", CmdCodecBase + 0, st.codec == 0, true}};
            if (caps->codecs & 2) codecs.push_back({L"LHDC", CmdCodecBase + 1, st.codec == 1, true});
            if (caps->codecs & 4) codecs.push_back({L"LDAC", CmdCodecBase + 2, st.codec == 2, true});
            h += segmented(x, yy + h, w, codecs);
            h += px(4);
        }
        if (supported(caps && caps->fitTest)) {
            h += px(8);
            int rh = px(50);
            text(x, yy + h + px(6), w - px(120), px(20), L"Ear tip fit test", P.text, uiFont);
            text(x, yy + h + px(26), w - px(120), px(18), fitText(), st.fitLeft == 0 && st.fitRight == 0 ? P.success : P.muted, uiSmall);
            button(rc(x + w - px(100), yy + h + px(10), px(100), px(30)), fitRunning ? L"Testing" : L"Run test", CmdFitTest, false, !fitRunning);
            h += rh;
        }
        return h;
    });

    if (caps && caps->dual && st.dualEnabled == 1 && !st.dual.empty()) {
        y += card(y, L"Connected devices", [&](int yy) {
            int h = 0;
            for (int i = 0; i < (int)st.dual.size(); ++i) {
                auto& d = st.dual[i];
                int rh = px(46);
                std::wstring name = d.self ? L"This PC" : d.name.empty() ? L"Unknown device" : settings::fromUtf8(d.name);
                text(x, yy + h + px(4), w - px(130), px(20), name, P.text, uiFont);
                text(x, yy + h + px(24), w - px(130), px(18), d.connected ? L"Connected" : L"Not connected", P.muted, uiSmall);
                if (!d.self) button(rc(x + w - px(110), yy + h + px(8), px(110), px(30)), d.connected ? L"Disconnect" : L"Connect", CmdDualDeviceBase + i);
                h += rh;
            }
            return h;
        });
    }

    y += card(y, L"About", [&](int yy) {
        int h = 0;
        auto row = [&](const std::wstring& k, const std::wstring& v) {
            text(x, yy + h, w / 2, px(24), k, P.muted, uiFont);
            text(x + w / 2, yy + h, w / 2, px(24), v, P.text, uiFont, DT_RIGHT);
            h += px(26);
        };
        row(L"Model", caps ? std::wstring(caps->name) + (fastPairInfo ? std::wstring(L" (") + fastPairInfo->colour + L")" : L"") : L"Unknown");
        row(L"Firmware", st.firmware.empty() ? L"--" : settings::fromUtf8(st.firmware));
        row(L"Bluetooth address", target.fake() ? L"test" : bt::addrString(target.addr));
        if (!modelId.empty()) row(L"Model ID", settings::fromUtf8(modelId));
        h += px(4);
        h += noteText(x, yy + h, w, L"Unofficial app, based on ear (web). Not affiliated with or endorsed by Nothing Technology Limited.", P.muted, uiSmall);
        return h;
    });
    return y;
}

void header(const RECT& client) {
    int hh = px(72);
    RECT r{client.left, client.top, client.right, client.top + hh};
    fill(C.dc, r, P.bg);
    int pad = px(20);
    int right = client.right - pad;
    // Right side: more, device menu, connect/disconnect.
    auto iconButton = [&](int cmd, const wchar_t* glyph) {
        RECT b{right - px(36), px(18), right, px(54)};
        if (hovered(cmd)) fillRounded(C.dc, b, blend(P.bg, P.text, pressed(cmd) ? 5 : 9), px(4));
        drawGlyph(C.dc, glyph, b, P.text);
        hits.push_back(Hit{b, cmd, 0, 0});
        right -= px(40);
    };
    iconButton(CmdMoreMenu, GlyphMore);
    iconButton(CmdDeviceMenu, GlyphBluetooth);
    if (conn == Connected) iconButton(CmdRefresh, GlyphRefresh);
    std::wstring status;
    COLORREF dot = P.muted;
    if (conn == Connected) {
        status = L"Connected";
        std::wstring b = batteryLine();
        if (!b.empty()) status += L"  ·  " + b;
        dot = P.success;
    } else if (conn == Connecting) { status = L"Connecting..."; dot = P.warning; }
    else if (awaitingReboot) { status = L"Rebooting..."; dot = P.warning; }
    else status = targetName.empty() ? L"Not connected" : L"Disconnected";
    int tw = right - pad - px(8);
    drawText(C.dc, deviceTitle(), RECT{pad, px(12), pad + tw, px(42)}, P.text, uiTitle);
    circle(C.dc, pad + px(4), px(52), px(8), dot);
    drawText(C.dc, status, RECT{pad + px(14), px(42), pad + tw, px(62)}, P.muted, uiSmall);
}

void render(HDC dc, const RECT& client) {
    hits.clear();
    fill(dc, client, P.bg);
    bodyTop = px(72);
    int maxW = px(560), margin = px(16) + px(CardPad);
    cardW = std::min<int>(client.right - 2 * margin, maxW - 2 * px(CardPad));
    cardX = (client.right - cardW) / 2;
    RECT bodyRect{0, bodyTop, client.right, client.bottom};
    C = Ctx{dc, false, bodyTop - scrollY, bodyRect};
    contentH = body(px(4)) + px(8);
    int view = client.bottom - bodyTop;
    scrollY = std::max(0, std::min(scrollY, contentH - view));
    scrollTarget = std::max(0, std::min(scrollTarget, contentH - view));
    C = Ctx{dc, true, bodyTop - scrollY, bodyRect};
    HRGN clip = CreateRectRgnIndirect(&bodyRect);
    SelectClipRgn(dc, clip);
    gCanvas.clip = bodyRect;
    body(px(4));
    // Scroll bar: a thin Fluent-style thumb that widens while hovered or dragged.
    scrollThumb = RECT{};
    if (contentH > view && view > 0) {
        int th = std::max(px(32), view * view / contentH);
        int ty = bodyTop + (view - th) * scrollY / std::max(1, contentH - view);
        scrollThumb = RECT{client.right - px(12), ty, client.right - px(2), ty + th};
        int tw = scrollHot || scrollDrag ? px(6) : px(3);
        RECT thumb{client.right - px(4) - tw, ty + px(2), client.right - px(4), ty + th - px(2)};
        fillRounded(dc, thumb, blend(P.bg, P.text, scrollHot || scrollDrag ? 50 : 35), tw / 2.0);
    }
    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);
    gCanvas.clip = client;
    C.draw = true;
    header(client);
}

void paint(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT client;
    GetClientRect(h, &client);
    // A top-down 32-bit DIB, so fillRounded() can blend anti-aliased edges straight into its pixels.
    int w = std::max<LONG>(1, client.right), hgt = std::max<LONG>(1, client.bottom);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -hgt;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, bmp);
    gCanvas = Canvas{(uint32_t*)bits, w, hgt, RECT{0, 0, w, hgt}};
    render(mem, client);
    GdiFlush();
    gCanvas = Canvas{};
    BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(h, &ps);
}

int hitAt(POINT p) { for (int i = (int)hits.size() - 1; i >= 0; --i) if (PtInRect(&hits[i].r, p)) return i; return -1; }

// ---------------------------------------------------------------- appearance and settings sync

bool systemDark() {
    DWORD v = 1, size = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &size) != ERROR_SUCCESS) return false;
    return v == 0;
}
void applyTheme() {
    HIGHCONTRASTW hc{sizeof hc};
    bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON);
    bool dark = !highContrast && (S.theme == 2 || S.theme == 3 || (S.theme == 0 && systemDark()));
    P = !dark ? LightPalette : S.theme == 3 ? AmoledPalette : DarkPalette;
    darkmode::app(dark);
    BOOL d = dark;
    DwmSetWindowAttribute(gMain, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &d, sizeof d);
    COLORREF caption = P.bg;
    DwmSetWindowAttribute(gMain, 35 /*DWMWA_CAPTION_COLOR, Windows 11*/, &caption, sizeof caption);
    invalidate();
}
void createFonts() {
    for (HFONT* f : {&uiFont, &uiBold, &uiTitle, &uiLarge, &uiSmall, &iconFont, &iconSmall}) if (*f) { DeleteObject(*f); *f = nullptr; }
    auto make = [](int height, int weight, const wchar_t* face) {
        return CreateFontW(height, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    };
    // The Windows 11 type ramp: body 14px, caption 12px, subtitle 20px.
    const wchar_t* face = textFace.c_str();
    uiFont = make(-px(14), FW_NORMAL, face);
    uiBold = make(-px(14), FW_SEMIBOLD, face);
    uiTitle = make(-px(20), FW_SEMIBOLD, face);
    uiLarge = make(-px(20), FW_SEMIBOLD, face);
    uiSmall = make(-px(12), FW_NORMAL, face);
    iconFont = make(-px(16), FW_NORMAL, iconFace.c_str());
    iconSmall = make(-px(12), FW_NORMAL, iconFace.c_str());
}
void pickIconFace() {
    HDC dc = GetDC(nullptr);
    HFONT f = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe Fluent Icons");
    HGDIOBJ o = SelectObject(dc, f);
    wchar_t face[LF_FACESIZE]{}; GetTextFaceW(dc, LF_FACESIZE, face);
    SelectObject(dc, o); DeleteObject(f); ReleaseDC(nullptr, dc);
    iconFace = wcscmp(face, L"Segoe Fluent Icons") == 0 ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    // Windows 11 ships Segoe UI Variable; earlier versions fall back to Segoe UI.
    dc = GetDC(nullptr);
    f = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI Variable Text");
    o = SelectObject(dc, f);
    GetTextFaceW(dc, LF_FACESIZE, face);
    SelectObject(dc, o); DeleteObject(f); ReleaseDC(nullptr, dc);
    textFace = wcscmp(face, L"Segoe UI Variable Text") == 0 ? L"Segoe UI Variable Text" : L"Segoe UI";
}
void saveNow() {
    KillTimer(gMain, TimerSave);
    if (gMain) {
        WINDOWPLACEMENT wp{sizeof wp};
        if (GetWindowPlacement(gMain, &wp)) { S.window = wp.rcNormalPosition; S.haveWindow = true; }
    }
    settings::save(gSettingsPath, S);
}

// ---------------------------------------------------------------- menus and commands

void execute(int cmd);
int popup(HMENU m, POINT pt, UINT align = TPM_LEFTALIGN) {
    SetForegroundWindow(gMain);
    int r = TrackPopupMenu(m, align | TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_TOPALIGN, pt.x, pt.y, 0, gMain, nullptr);
    DestroyMenu(m);
    return r;
}
void addItem(HMENU m, int id, const std::wstring& text, bool checked = false, bool enabled = true) {
    AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0) | (enabled ? 0 : MF_GRAYED), id, text.c_str());
}
void sep(HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
POINT belowHit(int cmd) {
    for (auto& h : hits) if (h.cmd == cmd) { POINT p{h.r.right, h.r.bottom}; ClientToScreen(gMain, &p); return p; }
    POINT p; GetCursorPos(&p); return p;
}

void showDeviceMenu() {
    refreshDevices();
    HMENU m = CreatePopupMenu(), other = CreatePopupMenu();
    int nothing = 0, rest = 0;
    for (int i = 0; i < (int)devices.size(); ++i) {
        auto& d = devices[i];
        std::wstring label = (d.name.empty() ? bt::addrString(d.addr) : d.name) + (d.connected ? L"\tconnected" : L"");
        bool current = conn != Idle && !target.fake() && target.addr == d.addr;
        if (models::looksLikeNothing(d.name)) { addItem(m, CmdDeviceBase + i, label, current); ++nothing; }
        else { addItem(other, CmdDeviceBase + i, label, current); ++rest; }
    }
    if (!nothing) addItem(m, 0, L"No Nothing or CMF devices paired", false, false);
    if (rest) AppendMenuW(m, MF_POPUP, (UINT_PTR)other, L"Other paired devices");
    else DestroyMenu(other);
    sep(m);
    if (conn != Idle) addItem(m, CmdDisconnect, L"Disconnect");
    else if (target.addr || target.fakePort) addItem(m, CmdConnect, L"Reconnect");
    addItem(m, CmdAutoConnect, L"Connect automatically", S.autoConnect);
    addItem(m, CmdBluetoothSettings, L"Bluetooth settings...");
    int r = popup(m, belowHit(CmdDeviceMenu), TPM_RIGHTALIGN);
    if (r) execute(r);
}
void showMoreMenu() {
    HMENU m = CreatePopupMenu(), theme = CreatePopupMenu(), model = CreatePopupMenu();
    const wchar_t* themes[] = {L"System", L"Light", L"Dark", L"AMOLED black"};
    for (int i = 0; i < 4; ++i) addItem(theme, CmdThemeBase + i, themes[i], S.theme == i);
    addItem(model, CmdModelBase - 1, L"Automatic (Fast Pair)", modelId.empty() || models::fastPair(modelId) != nullptr);
    sep(model);
    for (int i = 0; i < (int)(sizeof models::AllCaps / sizeof models::AllCaps[0]); ++i) {
        auto& c = models::AllCaps[i];
        addItem(model, CmdModelBase + i, std::wstring(c.name) + L"  (" + settings::fromUtf8(c.base) + L")", caps == &c && !models::fastPair(modelId), conn == Connected);
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)theme, L"Theme");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)model, L"Model");
    sep(m);
    addItem(m, CmdRefresh, L"Refresh\tF5", false, conn == Connected);
    addItem(m, CmdTray, L"Show in notification area", S.tray);
    addItem(m, CmdCloseToTray, L"Close to notification area", S.closeToTray, S.tray);
    addItem(m, CmdStartMinimized, L"Start minimised", S.startMinimized);
    addItem(m, CmdLowBattery, L"Low battery alerts", S.notifyLowBattery);
    sep(m);
    addItem(m, CmdSettingsFolder, L"Open settings folder");
    addItem(m, CmdAbout, L"About BudsPlus");
    addItem(m, CmdExit, L"Exit");
    int r = popup(m, belowHit(CmdMoreMenu), TPM_RIGHTALIGN);
    if (r) execute(r);
}
void showGestureMenu(int slot) {
    auto& slots = models::espeonSlots();
    if (slot < 0 || slot >= (int)slots.size()) return;
    auto& s = slots[slot];
    int a = gestureAction(S.gestureSide, s.type);
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < (int)s.actions.size(); ++i) {
        bool on = s.actions[i].wire == a || (s.actions[i].wire == 10 && models::isNoiseControlAction(a));
        addItem(m, CmdGestureBase + S.gestureSide * 400 + slot * 100 + i, s.actions[i].label, on);
    }
    if (s.type == 7 && models::isNoiseControlAction(a)) {
        bool v[3];
        models::noiseControlParts(a, v[0], v[1], v[2]);
        sep(m);
        addItem(m, 0, L"Noise control cycles through:", false, false);
        addItem(m, CmdNoiseCycleBase + 0, L"Transparency", v[0]);
        addItem(m, CmdNoiseCycleBase + 1, L"Noise cancellation", v[1]);
        addItem(m, CmdNoiseCycleBase + 2, L"Off", v[2]);
    }
    int r = popup(m, belowHit(CmdGestureSlotBase + slot), TPM_RIGHTALIGN);
    if (r) execute(r);
}
void showTrayMenu() {
    HMENU m = CreatePopupMenu();
    addItem(m, 0, deviceTitle() + (conn == Connected ? L"  " + batteryLine() : L""), false, false);
    sep(m);
    if (conn == Connected && caps && caps->ancLevel >= 0) {
        addItem(m, CmdAncBase + (proto::ancIsNoiseCancelling(st.anc) ? st.anc : proto::AncHigh), L"Noise cancelling", proto::ancIsNoiseCancelling(st.anc));
        if (caps->ancLevel & 0x10) addItem(m, CmdAncBase + proto::AncTransparency, L"Transparency", st.anc == proto::AncTransparency);
        addItem(m, CmdAncBase + proto::AncOff, L"Noise control off", st.anc == proto::AncOff);
        sep(m);
    }
    if (conn == Idle && (target.addr || target.fakePort)) addItem(m, CmdConnect, L"Connect");
    if (conn != Idle) addItem(m, CmdDisconnect, L"Disconnect");
    addItem(m, CmdShow, L"Open BudsPlus");
    addItem(m, CmdExit, L"Exit");
    POINT pt; GetCursorPos(&pt);
    int r = popup(m, pt, TPM_RIGHTALIGN | TPM_BOTTOMALIGN);
    if (r) execute(r);
}

void execute(int cmd) {
    if (cmd >= CmdDualDeviceBase) { toggleDualDevice(cmd - CmdDualDeviceBase); }
    else if (cmd >= CmdCustomEqBase) { int band = (cmd - CmdCustomEqBase) / 100, v = (cmd - CmdCustomEqBase) % 100 - 6; if (band < 3) setCustomEq(band, v); }
    else if (cmd >= CmdGestureBase) {
        int rel = cmd - CmdGestureBase, side = rel / 400, slot = rel % 400 / 100, opt = rel % 100;
        auto& slots = models::espeonSlots();
        if (side < 2 && slot < (int)slots.size() && opt < (int)slots[slot].actions.size()) {
            int a = slots[slot].actions[opt].wire;
            if (a == 10) {
                // Keep the cycle the other earbud already uses, so both stay in step.
                int other = gestureAction(1 - side, 7);
                if (models::isNoiseControlAction(other)) a = other;
            }
            setGesture(side, slots[slot].type, a);
        }
    }
    else if (cmd >= CmdNoiseCycleBase && cmd < CmdNoiseCycleBase + 3) toggleNoiseCycle(cmd - CmdNoiseCycleBase);
    else if (cmd >= CmdGestureSlotBase && cmd < CmdGestureSlotBase + 10) showGestureMenu(cmd - CmdGestureSlotBase);
    else if (cmd >= CmdGestureSideBase && cmd < CmdGestureSideBase + 2) { S.gestureSide = cmd - CmdGestureSideBase; scheduleSave(); }
    else if (cmd >= CmdRingLeft && cmd <= CmdRingBoth) ring(cmd - CmdRingLeft);
    else if (cmd == CmdFitTest) startFitTest();
    else if (cmd >= CmdCodecBase && cmd < CmdCodecBase + 3) setCodec(cmd - CmdCodecBase);
    else if (cmd == CmdInEar) { if (st.inEar >= 0) { st.inEar = !st.inEar; send(proto::SetInEar, {0x01, 0x01, (uint8_t)st.inEar}); } }
    else if (cmd == CmdLatency) { if (st.latency >= 0) { st.latency = !st.latency; send(proto::SetLatency, {(uint8_t)(st.latency ? 1 : 2), 0x00}); } }
    else if (cmd == CmdPersonal) { if (st.personal >= 0) setPersonal(!st.personal); }
    else if (cmd == CmdDual) { if (st.dualEnabled >= 0) setDualEnabled(!st.dualEnabled); }
    else if (cmd >= CmdSpatialBase && cmd < CmdSpatialBase + 6) setSpatial(cmd - CmdSpatialBase);
    else if (cmd >= CmdBassLevelBase && cmd < CmdBassLevelBase + 10) setBassLevel(cmd - CmdBassLevelBase);
    else if (cmd == CmdBassToggle) {
        if (st.bassOn != 1 && spatialExcludesBass() && st.spatialMode > 0) info(L"Bass Enhance", L"Bass Enhance is not available while spatial audio is on.");
        else { st.bassOn = st.bassOn == 1 ? 0 : 1; sendBass(); }
    }
    else if (cmd >= CmdEqBase && cmd < CmdEqBase + 10) setEqPreset(cmd - CmdEqBase);
    else if (cmd >= CmdAncBase && cmd < CmdAncBase + 10) setAnc(cmd - CmdAncBase);
    else if (cmd >= CmdThemeBase && cmd < CmdThemeBase + 4) { S.theme = cmd - CmdThemeBase; applyTheme(); scheduleSave(); }
    else if (cmd >= CmdModelBase - 1 && cmd < CmdModelBase + 100) {
        std::wstring key = target.fake() ? L"fake" : bt::addrString(target.addr);
        if (cmd == CmdModelBase - 1) { S.models.erase(key); if (conn != Idle) connectTo(target, targetName); }
        else {
            auto& c = models::AllCaps[cmd - CmdModelBase];
            S.models[key] = settings::fromUtf8(c.base);
            modelId = c.base;
            fastPairInfo = nullptr;
            caps = &c;
            modelGuessed = false;
            if (conn == Connected) { queueInit(); SetTimer(gMain, TimerInit, 90, nullptr); }
        }
        scheduleSave();
    }
    else if (cmd >= CmdDeviceBase && cmd < CmdDeviceBase + 1000) { int i = cmd - CmdDeviceBase; if (i < (int)devices.size()) connectDevice(devices[i]); }
    else switch (cmd) {
    case CmdConnect: if (target.addr || target.fakePort) connectTo(target, targetName); break;
    case CmdDisconnect: case CmdCancel: disconnect(true); if (cmd == CmdCancel) statusMessage.clear(); break;
    case CmdRefresh: if (conn == Connected) { queueInit(); SetTimer(gMain, TimerInit, 90, nullptr); } else refreshDevices(); break;
    case CmdDeviceMenu: showDeviceMenu(); break;
    case CmdMoreMenu: showMoreMenu(); break;
    case CmdShowAll: showAllDevices = !showAllDevices; refreshDevices(); break;
    case CmdBluetoothSettings: ShellExecuteW(gMain, L"open", L"ms-settings:bluetooth", nullptr, nullptr, SW_SHOWNORMAL); break;
    case CmdAutoConnect: S.autoConnect = !S.autoConnect; scheduleSave(); break;
    case CmdTray: S.tray = !S.tray; setTray(S.tray); scheduleSave(); break;
    case CmdCloseToTray: S.closeToTray = !S.closeToTray; scheduleSave(); break;
    case CmdStartMinimized: S.startMinimized = !S.startMinimized; scheduleSave(); break;
    case CmdLowBattery: S.notifyLowBattery = !S.notifyLowBattery; scheduleSave(); break;
    case CmdSettingsFolder: saveNow(); ShellExecuteW(gMain, L"open", settings::folderOf(gSettingsPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case CmdAbout:
        info(L"About BudsPlus", L"BudsPlus 1.0\nNative Windows control app for Nothing and CMF earbuds and headphones.\n\n"
                                L"A desktop port of ear (web) by the radiance project (github.com/radiance-project/ear-web).\n"
                                L"Unofficial: not affiliated with or endorsed by Nothing Technology Limited.");
        break;
    case CmdShow: showMain(); break;
    case CmdExit: reallyQuit = true; DestroyWindow(gMain); break;
    }
    invalidate();
    updateTray();
}

// ---------------------------------------------------------------- window procedure

int sliderValueAt(int band, int x) {
    RECT t = sliderTrack[band];
    int w = std::max<LONG>(1, t.right - t.left);
    int v = (int)lround((double)(x - t.left) * 12 / w) - 6;
    return std::max(-6, std::min(6, v));
}
// Wheel and key scrolling glide to the new position over a few frames instead of jumping.
void scrollBy(int dy) {
    scrollTarget = std::max(0, scrollTarget + dy);   // render() clamps it to the content
    SetTimer(gMain, TimerScroll, 10, nullptr);
}
void scrollTo(int y) { scrollY = scrollTarget = std::max(0, y); KillTimer(gMain, TimerScroll); invalidate(); }
void scrollStep() {
    int d = scrollTarget - scrollY;
    if (std::abs(d) <= 1) { scrollY = scrollTarget; KillTimer(gMain, TimerScroll); }
    else scrollY += d > 0 ? std::max(1, d * 3 / 10) : std::min(-1, d * 3 / 10);
    invalidate();
}

LRESULT test(WPARAM q) {
    switch (q) {
    case 1: return conn;
    case 2: return st.anc;
    case 3: return st.eqMode;
    case 4: return st.left.level;
    case 5: return st.right.level;
    case 6: return st.caseB.level;
    case 7: return st.bassOn;
    case 8: return st.bassWire;
    case 9: return st.inEar;
    case 10: return st.latency;
    case 11: case 12: case 13: return st.custom[q - 11];
    case 14: return st.spatialMode < 0 ? -1 : models::spatialIndex(st.spatialMode, st.spatialHead);
    case 15: return st.codec;
    case 16: return st.dualEnabled;
    case 17: return (LRESULT)st.dual.size();
    case 18: return st.personal;
    case 19: return (LRESULT)hits.size();
    case 20: return contentH;
    case 21: return st.fitLeft * 10 + st.fitRight;
    case 22: return caps ? 1 : 0;
    case 23: return (LRESULT)st.firmware.size();
    case 24: return st.left.charging;
    case 25: return S.gestureSide;
    case 26: return S.theme;
    }
    if (q >= 1000 && q < 1400) return gestureAction((int)(q - 1000) / 100, (int)(q - 1000) % 100);
    return -1;
}

LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TASKBARCREATED && WM_TASKBARCREATED) { trayAdded = false; setTray(S.tray); return 0; }
    switch (m) {
    case WM_CREATE: gMain = h; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: paint(h); return 0;
    case WM_SIZE: invalidate(); return 0;
    case WM_ACTIVATE: if (LOWORD(w) != WA_INACTIVE && conn == Idle) { refreshDevices(); invalidate(); } break;
    case WM_GETMINMAXINFO: { auto mm = (MINMAXINFO*)l; mm->ptMinTrackSize.x = px(380); mm->ptMinTrackSize.y = px(420); return 0; }
    case WM_DPICHANGED: {
        dpi = HIWORD(w);
        createFonts();
        RECT* r = (RECT*)l;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        invalidate();
        return 0;
    }
    case WM_SETTINGCHANGE: if (l && wcscmp((const wchar_t*)l, L"ImmersiveColorSet") == 0 && S.theme == 0) applyTheme(); return 0;
    case WM_MOUSEMOVE: {
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (dragBand >= 0) {
            int v = sliderValueAt(dragBand, p.x);
            if (v != dragValue) { dragValue = v; invalidate(); }
            return 0;
        }
        if (scrollDrag) {
            RECT c; GetClientRect(h, &c);
            int view = c.bottom - bodyTop, th = scrollThumb.bottom - scrollThumb.top;
            if (view > th) scrollTo(scrollDragOrigin + (p.y - scrollDragStart) * (contentH - view) / (view - th));
            invalidate();
            return 0;
        }
        int hit = hitAt(p);
        bool overThumb = PtInRect(&scrollThumb, p) != 0;
        if (hit != hoverHit || overThumb != scrollHot) { hoverHit = hit; scrollHot = overThumb; invalidate(); }
        TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        SetCursor(LoadCursor(nullptr, hit >= 0 ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_SETCURSOR: if (LOWORD(l) == HTCLIENT) { SetCursor(LoadCursor(nullptr, hoverHit >= 0 ? IDC_HAND : IDC_ARROW)); return TRUE; } break;
    case WM_MOUSELEAVE: if ((hoverHit >= 0 || scrollHot) && dragBand < 0) { hoverHit = -1; scrollHot = false; invalidate(); } return 0;
    case WM_LBUTTONDOWN: {
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (PtInRect(&scrollThumb, p)) { scrollDrag = true; scrollDragStart = p.y; scrollDragOrigin = scrollY; KillTimer(h, TimerScroll); scrollTarget = scrollY; SetCapture(h); return 0; }
        pressHit = hitAt(p);
        if (pressHit >= 0 && hits[pressHit].kind == 1) {
            dragBand = hits[pressHit].arg;
            dragValue = sliderValueAt(dragBand, p.x);
        }
        SetCapture(h);
        invalidate();
        return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (scrollDrag) { scrollDrag = false; invalidate(); return 0; }
        if (dragBand >= 0) {
            int band = dragBand, v = dragValue;
            dragBand = -1;
            pressHit = -1;
            if (v != st.custom[band]) execute(CmdCustomEqBase + band * 100 + v + 6);
            invalidate();
            return 0;
        }
        int hit = hitAt(p);
        int cmd = hit >= 0 && hit == pressHit ? hits[hit].cmd : 0;
        pressHit = -1;
        if (cmd) execute(cmd);
        invalidate();
        return 0;
    }
    case WM_MOUSEWHEEL: scrollBy(-GET_WHEEL_DELTA_WPARAM(w) * px(48) / WHEEL_DELTA); return 0;
    case WM_KEYDOWN:
        if (w == VK_F5) execute(CmdRefresh);
        else if (w == VK_DOWN) scrollBy(px(40));
        else if (w == VK_UP) scrollBy(-px(40));
        else if (w == VK_NEXT) scrollBy(px(300));
        else if (w == VK_PRIOR) scrollBy(-px(300));
        else if (w == VK_HOME) scrollTo(0);
        else if (w == VK_END) scrollTo(1 << 20);
        return 0;
    case WM_COMMAND: execute(LOWORD(w) | (HIWORD(w) && !l ? 0 : 0)); return 0;
    case WM_LINK: onLinkEvents(); return 0;
    case WM_TESTQUERY: return test(w);
    case WM_TIMER:
        switch (w) {
        case TimerScroll: scrollStep(); return 0;
        case TimerInit:
            if (initQueue.empty() || conn != Connected) { KillTimer(h, TimerInit); return 0; }
            send(initQueue.front().first, initQueue.front().second);
            initQueue.pop_front();
            return 0;
        case TimerReconnect:
            KillTimer(h, TimerReconnect);
            if (conn == Idle && (target.addr || target.fakePort)) {
                int attempts = reconnectAttempts;
                bool reboot = awaitingReboot;
                connectTo(target, targetName);
                reconnectAttempts = attempts;
                awaitingReboot = reboot;
            }
            return 0;
        case TimerBattery: send(proto::GetBattery); return 0;
        case TimerSave: saveNow(); return 0;
        case TimerFit: KillTimer(h, TimerFit); fitRunning = false; invalidate(); return 0;
        }
        return 0;
    case WM_POWERBROADCAST:
        if (w == PBT_APMRESUMEAUTOMATIC && conn == Idle && S.autoConnect && !userDisconnected && (target.addr || target.fakePort)) scheduleReconnect(4);
        return TRUE;
    case WM_TRAY:
        switch (LOWORD(l)) {
        case WM_LBUTTONUP: case NIN_SELECT: case NIN_KEYSELECT:
            if (IsWindowVisible(h) && !IsIconic(h) && GetForegroundWindow() == h) ShowWindow(h, SW_HIDE);
            else showMain();
            return 0;
        case WM_CONTEXTMENU: case WM_RBUTTONUP: showTrayMenu(); return 0;
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((w & 0xFFF0) == SC_MINIMIZE && trayAdded && S.closeToTray) { ShowWindow(h, SW_HIDE); return 0; }
        break;
    case WM_CLOSE:
        if (!reallyQuit && trayAdded && S.closeToTray) { ShowWindow(h, SW_HIDE); return 0; }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        saveNow();
        setTray(false);
        gLink.stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

std::wstring instanceKey() {
    std::wstring p = gSettingsPath;
    for (auto& c : p) c = (wchar_t)towlower(c);
    uint32_t hash = 2166136261u;
    for (wchar_t c : p) { hash ^= (uint32_t)c; hash *= 16777619u; }
    wchar_t b[16]; swprintf(b, 16, L"%08X", hash);
    return b;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    gInst = inst;
    using SetDpiCtxFn = BOOL(WINAPI*)(HANDLE);
    if (auto f = (SetDpiCtxFn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")) f((HANDLE)(INT_PTR)-4);
    gTest = GetEnvironmentVariableW(L"BUDSPLUS_TEST", nullptr, 0) > 0;
    static char fake[512];
    DWORD fn = GetEnvironmentVariableA("BUDSPLUS_FAKE", fake, sizeof fake);
    if (fn && fn < sizeof fake) gFakeSpec = fake;
    wchar_t logPath[MAX_PATH];
    DWORD ln = GetEnvironmentVariableW(L"BUDSPLUS_LOG", logPath, MAX_PATH);
    if (ln && ln < MAX_PATH) gLog = _wfopen(logPath, L"a");
    bool minimized = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) if (wcscmp(argv[i], L"--minimized") == 0) minimized = true;
    LocalFree(argv);

    gSettingsPath = settings::defaultPath();
    std::wstring key = instanceKey();
    HANDLE mutex = CreateMutexW(nullptr, FALSE, (L"Local\\BudsPlus-" + key).c_str());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = nullptr;
        for (int i = 0; i < 40 && !other; ++i) {
            for (HWND hw = FindWindowExW(nullptr, nullptr, MainClass, nullptr); hw; hw = FindWindowExW(nullptr, hw, MainClass, nullptr)) {
                wchar_t prop[32]{};
                if (GetPropW(hw, (L"BudsPlus-" + key).c_str())) { other = hw; break; }
                (void)prop;
            }
            if (!other) Sleep(50);
        }
        if (other) {
            DWORD pid = 0; GetWindowThreadProcessId(other, &pid); AllowSetForegroundWindow(pid);
            PostMessageW(other, WM_COMMAND, CmdShow, 0);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    S = settings::load(gSettingsPath);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    darkmode::init();
    pickIconFace();
    { HDC screen = GetDC(nullptr); dpi = GetDeviceCaps(screen, LOGPIXELSY); ReleaseDC(nullptr, screen); }
    createFonts();
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{sizeof wc};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = MainClass;
    RegisterClassExW(&wc);

    int ww = px(480), wh = px(820);
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (S.haveWindow) {
        RECT r = S.window;
        HMONITOR mon = MonitorFromRect(&r, MONITOR_DEFAULTTONULL);
        if (mon) { x = r.left; y = r.top; ww = r.right - r.left; wh = r.bottom - r.top; }
    }
    HWND h = CreateWindowExW(0, MainClass, AppName, WS_OVERLAPPEDWINDOW, x, y, ww, wh, nullptr, nullptr, inst, nullptr);
    if (!h) return 1;
    SetPropW(h, (L"BudsPlus-" + key).c_str(), (HANDLE)1);
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    if (auto f = (GetDpiForWindowFn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")) {
        UINT d = f(h);
        if (d && (int)d != dpi) {
            dpi = (int)d;
            createFonts();
            if (!S.haveWindow) SetWindowPos(h, nullptr, 0, 0, px(480), px(820), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    gLink.setNotify(h, WM_LINK);
    applyTheme();
    setTray(S.tray);
    refreshDevices();

    bool hidden = (minimized || S.startMinimized) && S.tray && !gTest;
    if (gTest) ShowWindow(h, SW_SHOWNOACTIVATE);
    else if (!hidden) ShowWindow(h, (minimized || S.startMinimized) ? SW_SHOWMINNOACTIVE : show);
    UpdateWindow(h);

    // Start-up connection: the fake device in tests, else the last device, else a connected Nothing device.
    if (gFakeSpec) {
        bt::Target t;
        if (bt::parseFake(gFakeSpec, t)) connectTo(t, L"CMF Buds 2 Plus (test)");
    } else if (S.autoConnect) {
        uint64_t last = bt::parseAddr(S.device);
        const bt::Device* pick = nullptr;
        for (auto& d : devices) if (last && d.addr == last) pick = &d;
        if (!pick) {
            int nothingConnected = 0;
            for (auto& d : devices) if (d.connected && models::looksLikeNothing(d.name)) { if (!pick) pick = &d; ++nothingConnected; }
            if (nothingConnected != 1) pick = nullptr;
        }
        if (pick) connectDevice(*pick);
        else if (last) { bt::Target t; t.addr = last; target = t; }
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    WSACleanup();
    if (mutex) CloseHandle(mutex);
    return 0;
}
