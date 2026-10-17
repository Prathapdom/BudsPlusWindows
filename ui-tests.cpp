// UI integration tests: runs BudsPlus.exe against the fake earbuds (fakebuds.hpp) and drives it with
// WM_COMMAND, checking both what the app shows (WM_TESTQUERY) and what the "earbuds" were sent.
// Usage: ui-tests.exe path\to\BudsPlus.exe
#include "fakebuds.hpp"
#include "settings.hpp"
#include <cstdio>
#include <string>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

const UINT WM_TESTQUERY = WM_APP + 100;
enum Q { QConn = 1, QAnc, QEq, QLeft, QRight, QCase, QBassOn, QBassWire, QInEar, QLatency, QBass, QMid, QTreble, QSpatial, QCodec,
         QDual, QDualCount, QPersonal, QHits, QContentH, QFit, QCaps, QFirmware, QCharging, QGestureSide, QTheme };
enum Cmd { CmdDisconnect = 101, CmdConnect = 100, CmdExit = 114, CmdThemeBase = 3100, CmdAncBase = 5000, CmdEqBase = 5100, CmdBassToggle = 5200,
           CmdBassLevelBase = 5210, CmdSpatialBase = 5300, CmdInEar = 5400, CmdLatency, CmdPersonal, CmdDual, CmdCodecBase = 5500,
           CmdFitTest = 5600, CmdRingLeft = 5610, CmdGestureSideBase = 5700, CmdNoiseCycleBase = 5750, CmdGestureBase = 6000,
           CmdCustomEqBase = 7000, CmdDualDeviceBase = 8000 };

HWND app = nullptr;
DWORD appPid = 0;

static BOOL CALLBACK findMain(HWND h, LPARAM) {
    wchar_t cls[64];
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == appPid && GetClassNameW(h, cls, 64) && wcscmp(cls, L"BudsPlus.Main") == 0) { app = h; return FALSE; }
    return TRUE;
}
static LRESULT q(int what) { return SendMessageW(app, WM_TESTQUERY, (WPARAM)what, 0); }
static void cmd(int c) { SendMessageW(app, WM_COMMAND, (WPARAM)c, 0); }
template <class F> static bool waitFor(F cond, int ms = 4000) {
    for (int i = 0; i < ms / 20; ++i) { if (cond()) return true; Sleep(20); }
    return cond();
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { printf("usage: ui-tests BudsPlus.exe\n"); return 2; }
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    FakeBuds fake;
    if (!fake.start()) { printf("fake device: cannot listen\n"); return 1; }

    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"BudsPlusUiTest", ini = dir + L"\\settings.ini";
    DeleteFileW(ini.c_str());
    CreateDirectoryW(dir.c_str(), nullptr);
    settings::writeFile(ini, "[ui]\r\ntray = 0\r\n");
    SetEnvironmentVariableW(L"BUDSPLUS_SETTINGS", ini.c_str());
    SetEnvironmentVariableW(L"BUDSPLUS_TEST", L"1");
    SetEnvironmentVariableA("BUDSPLUS_FAKE", ("127.0.0.1:" + std::to_string(fake.port) + ":5C587F").c_str());

    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    std::wstring line = L"\"" + std::wstring(argv[1]) + L"\"";
    if (!CreateProcessW(argv[1], &line[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { printf("cannot start %ls\n", argv[1]); return 1; }
    appPid = pi.dwProcessId;
    WaitForInputIdle(pi.hProcess, 5000);
    waitFor([] { EnumWindows(findMain, 0); return app != nullptr; });
    CHECK(app != nullptr);
    if (!app) { TerminateProcess(pi.hProcess, 1); return 1; }

    // Connect and run the start-up queries.
    CHECK(waitFor([] { return q(QConn) == 2; }));
    CHECK(waitFor([] { return q(QPersonal) >= 0; }, 6000));   // the last query of the init queue was answered
    CHECK(q(QCaps) == 1);
    CHECK(q(QLeft) == 82 && q(QRight) == 77 && q(QCase) == 54 && q(QCharging) == 0);
    CHECK(q(QAnc) == proto::AncHigh && q(QEq) == 3 && q(QBassOn) == 1 && q(QBassWire) == 4);
    CHECK(q(QInEar) == 1 && q(QLatency) == 0 && q(QCodec) == 0 && q(QSpatial) == 0 && q(QDual) == 1 && q(QFirmware) == 9);
    CHECK(waitFor([] { return q(QDualCount) == 2; }));
    CHECK(q(1000 + 2) == 2 && q(1100 + 3) == 9);   // left double tap play/pause, right triple tap skip forward
    { std::lock_guard<std::mutex> g(fake.m); CHECK(fake.timeSets == 1 && fake.connections == 1); }
    InvalidateRect(app, nullptr, FALSE);
    UpdateWindow(app);
    CHECK(q(QHits) > 20 && q(QContentH) > 800);

    auto fakeIs = [&](auto get, int want) { return waitFor([&] { std::lock_guard<std::mutex> g(fake.m); return get() == want; }); };

    // Noise control.
    cmd(CmdAncBase + proto::AncTransparency);
    CHECK(fakeIs([&] { return fake.anc; }, proto::AncTransparency) && q(QAnc) == proto::AncTransparency);
    cmd(CmdAncBase + proto::AncAdaptive);
    CHECK(fakeIs([&] { return fake.anc; }, proto::AncAdaptive));
    { std::lock_guard<std::mutex> g(fake.m); CHECK(fake.lastPayload.size() == 3 && fake.lastPayload[0] == 1 && fake.lastPayload[2] == 0); }

    // EQ presets and the custom EQ.
    cmd(CmdEqBase + 5);
    CHECK(fakeIs([&] { return fake.listening; }, 5) && q(QEq) == 5);
    cmd(CmdCustomEqBase + 0 * 100 + (4 + 6));    // bass +4 switches to the custom preset
    cmd(CmdCustomEqBase + 2 * 100 + (-3 + 6));   // treble -3
    CHECK(fakeIs([&] { return fake.listening; }, 6) && q(QEq) == 6);
    CHECK(waitFor([&] {
        std::lock_guard<std::mutex> g(fake.m);
        proto::Packet pk; pk.command = proto::RspCustomEq; pk.raw = proto::Bytes(8, 0);
        pk.raw.insert(pk.raw.end(), fake.customEq.begin(), fake.customEq.end());
        proto::State s; proto::apply(s, pk);
        return s.custom[0] == 4 && s.custom[1] == 0 && s.custom[2] == -3;
    }));
    CHECK(q(QBass) == 4 && q(QMid) == 0 && q(QTreble) == -3);
    cmd(CmdCustomEqBase + 1 * 100 + (99));   // out of range: clamped, not sent as garbage
    CHECK(q(QMid) >= -6 && q(QMid) <= 6);

    // Ultra Bass: level 3 is wire 6; off keeps the level.
    cmd(CmdBassLevelBase + 3);
    CHECK(fakeIs([&] { return fake.bassWire; }, 6));
    cmd(CmdBassToggle);
    CHECK(fakeIs([&] { return fake.bassOn; }, 0) && q(QBassOn) == 0);
    { std::lock_guard<std::mutex> g(fake.m); CHECK(fake.bassWire == 6); }
    cmd(CmdBassToggle);
    CHECK(fakeIs([&] { return fake.bassOn; }, 1));

    // Spatial audio: "Fixed" is mode 1 without head tracking.
    cmd(CmdSpatialBase + 2);
    CHECK(fakeIs([&] { return fake.spatialMode; }, 1) && fakeIs([&] { return fake.spatialHead; }, 0) && q(QSpatial) == 2);

    // Toggles.
    cmd(CmdInEar);
    CHECK(fakeIs([&] { return fake.inEar; }, 0) && q(QInEar) == 0);
    cmd(CmdLatency);
    CHECK(fakeIs([&] { return fake.latency; }, 1) && q(QLatency) == 1);
    cmd(CmdLatency);
    CHECK(fakeIs([&] { return fake.latency; }, 2));
    cmd(CmdPersonal);
    CHECK(fakeIs([&] { return fake.personal; }, 1));

    // Gestures: right earbud, triple tap (slot 1) -> "Voice assistant" (option 2, action 11).
    cmd(CmdGestureSideBase + 1);
    CHECK(q(QGestureSide) == 1);
    cmd(CmdGestureBase + 1 * 400 + 1 * 100 + 2);
    CHECK(fakeIs([&] { for (auto& x : fake.gestures) if (x.device == 3 && x.type == 3) return (int)x.action; return -1; }, 11));
    CHECK(q(1100 + 3) == 11);
    // Tap-and-hold noise-control cycle: dropping "Off" from {transparency, NC, off} gives code 22 on both buds.
    cmd(CmdNoiseCycleBase + 2);
    CHECK(fakeIs([&] { for (auto& x : fake.gestures) if (x.device == 2 && x.type == 7) return (int)x.action; return -1; }, 22));
    CHECK(fakeIs([&] { for (auto& x : fake.gestures) if (x.device == 3 && x.type == 7) return (int)x.action; return -1; }, 22));
    // ...and dropping another mode is refused (the cycle needs two).
    cmd(CmdNoiseCycleBase + 1);
    Sleep(150);
    CHECK(q(1000 + 7) == 22);

    // Ring the left earbud, then stop.
    cmd(CmdRingLeft);
    CHECK(fakeIs([&] { return fake.ringLeft; }, 1));
    cmd(CmdRingLeft);
    CHECK(fakeIs([&] { return fake.ringLeft; }, 0));

    // Ear fit test: the fake answers "left good, right adjust".
    cmd(CmdFitTest);
    CHECK(waitFor([] { return q(QFit) == 1; }));

    // Dual connection: connect the phone, then a pushed event updates the list.
    cmd(CmdDualDeviceBase + 1);
    CHECK(fakeIs([&] { return (int)fake.dualConnected[1]; }, 1));

    // Pushed battery update, with the left bud charging.
    { std::lock_guard<std::mutex> g(fake.m); fake.left = 12; fake.leftCharging = true; }
    fake.pushBattery();
    CHECK(waitFor([] { return q(QLeft) == 12 && q(QCharging) == 1; }));

    // Codec change reboots the earbuds: the app reconnects by itself and reads everything again.
    cmd(CmdCodecBase + 2);
    CHECK(fakeIs([&] { return fake.codec; }, 2));
    CHECK(waitFor([] { return q(QConn) != 2; }, 3000));
    CHECK(waitFor([] { return q(QConn) == 2 && q(QCodec) == 2; }, 15000));
    { std::lock_guard<std::mutex> g(fake.m); CHECK(fake.connections == 2); }

    // Disconnect by hand: no automatic reconnection.
    cmd(CmdDisconnect);
    CHECK(q(QConn) == 0);
    Sleep(700);
    CHECK(q(QConn) == 0);
    cmd(CmdConnect);
    CHECK(waitFor([] { return q(QConn) == 2; }));

    // Theme and settings persistence.
    cmd(CmdThemeBase + 3);
    CHECK(q(QTheme) == 3);
    InvalidateRect(app, nullptr, FALSE);
    UpdateWindow(app);
    PostMessageW(app, WM_COMMAND, CmdExit, 0);
    CHECK(WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CHECK(code == 0);
    settings::Settings s = settings::load(ini);
    CHECK(s.theme == 3 && s.gestureSide == 1 && s.haveWindow && !s.tray);
    CHECK(s.models.count(L"fake") && s.models[L"fake"] == L"5C587F");

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    fake.stop();
    DeleteFileW(ini.c_str());
    RemoveDirectoryW(dir.c_str());
    WSACleanup();
    printf("%d UI checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
