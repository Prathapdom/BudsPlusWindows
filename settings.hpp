// BudsPlus settings, stored as a UTF-8 INI-style text file (not the registry):
//   %APPDATA%\BudsPlus\settings.ini   (or the path in the BUDSPLUS_SETTINGS environment variable)
#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace settings {

struct Settings {
    int theme = 0;                 // 0 system, 1 light, 2 dark, 3 AMOLED black
    bool haveWindow = false;
    RECT window{};
    std::wstring device;           // Bluetooth address of the last device, "AA:BB:CC:DD:EE:FF"
    bool autoConnect = true;       // connect to it at start-up and reconnect when it drops
    bool tray = true;              // show the battery in the notification area
    bool closeToTray = false;
    bool startMinimized = false;
    bool notifyLowBattery = true;
    int gestureSide = 0;           // 0 left, 1 right
    std::map<std::wstring, std::wstring> models;   // address -> Fast Pair model ID, remembered from the last connection
};

inline std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
inline std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
inline std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n"), b = s.find_last_not_of(L" \t\r\n");
    return a == std::wstring::npos ? std::wstring() : s.substr(a, b - a + 1);
}

inline std::wstring defaultPath() {
    wchar_t env[32768];
    DWORD n = GetEnvironmentVariableW(L"BUDSPLUS_SETTINGS", env, 32768);
    if (n && n < 32768) return env;
    PWSTR appData = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) dir = appData;
    CoTaskMemFree(appData);
    if (dir.empty()) { wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); dir = tmp; }
    return dir + L"\\BudsPlus\\settings.ini";
}
inline std::wstring folderOf(const std::wstring& p) { size_t i = p.find_last_of(L"\\/"); return i == std::wstring::npos ? L"." : p.substr(0, i); }

inline bool readFile(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    out.assign((size_t)std::min<LONGLONG>(size.QuadPart, 1 << 20), '\0');
    DWORD got = 0;
    BOOL ok = out.empty() || ReadFile(f, &out[0], (DWORD)out.size(), &got, nullptr);
    CloseHandle(f);
    out.resize(got);
    return ok != FALSE;
}
// Writes through a temporary file and a rename, so a crash never leaves a half-written file.
inline bool writeFile(const std::wstring& path, const std::string& data) {
    SHCreateDirectoryExW(nullptr, folderOf(path).c_str(), nullptr);
    std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    BOOL ok = data.empty() || WriteFile(f, data.data(), (DWORD)data.size(), &put, nullptr);
    CloseHandle(f);
    if (!ok || put != data.size()) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

inline bool parseBool(const std::wstring& v) { return v == L"1" || v == L"true" || v == L"yes" || v == L"on"; }

inline Settings load(const std::wstring& path) {
    Settings s;
    std::string raw;
    if (!readFile(path, raw)) return s;
    if (raw.size() >= 3 && (uint8_t)raw[0] == 0xEF && (uint8_t)raw[1] == 0xBB && (uint8_t)raw[2] == 0xBF) raw.erase(0, 3);
    std::wstring text = fromUtf8(raw), section;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find(L'\n', pos);
        if (end == std::wstring::npos) end = text.size();
        std::wstring ln = trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (ln.empty() || ln[0] == L';' || ln[0] == L'#') continue;
        if (ln.front() == L'[' && ln.back() == L']') { section = ln.substr(1, ln.size() - 2); continue; }
        size_t eq = ln.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = trim(ln.substr(0, eq)), v = trim(ln.substr(eq + 1));
        if (section == L"models") { if (!k.empty() && !v.empty()) s.models[k] = v; continue; }
        int iv = _wtoi(v.c_str());
        if (k == L"theme") s.theme = iv >= 0 && iv <= 3 ? iv : 0;
        else if (k == L"window") {
            long a[4];
            if (swscanf(v.c_str(), L"%ld,%ld,%ld,%ld", &a[0], &a[1], &a[2], &a[3]) == 4 && a[2] > a[0] && a[3] > a[1]) {
                s.window = RECT{a[0], a[1], a[2], a[3]};
                s.haveWindow = true;
            }
        }
        else if (k == L"device") s.device = v;
        else if (k == L"autoConnect") s.autoConnect = parseBool(v);
        else if (k == L"tray") s.tray = parseBool(v);
        else if (k == L"closeToTray") s.closeToTray = parseBool(v);
        else if (k == L"startMinimized") s.startMinimized = parseBool(v);
        else if (k == L"notifyLowBattery") s.notifyLowBattery = parseBool(v);
        else if (k == L"gestureSide") s.gestureSide = iv == 1 ? 1 : 0;
    }
    return s;
}

inline bool save(const std::wstring& path, const Settings& s) {
    std::wstring t = L"; BudsPlus settings\r\n[ui]\r\n";
    auto put = [&](const wchar_t* k, const std::wstring& v) { t += k; t += L" = "; t += v; t += L"\r\n"; };
    auto flag = [&](const wchar_t* k, bool v) { put(k, v ? L"1" : L"0"); };
    put(L"theme", std::to_wstring(s.theme));
    if (s.haveWindow) {
        wchar_t b[96];
        swprintf(b, 96, L"%ld,%ld,%ld,%ld", s.window.left, s.window.top, s.window.right, s.window.bottom);
        put(L"window", b);
    }
    flag(L"tray", s.tray);
    flag(L"closeToTray", s.closeToTray);
    flag(L"startMinimized", s.startMinimized);
    flag(L"notifyLowBattery", s.notifyLowBattery);
    put(L"gestureSide", std::to_wstring(s.gestureSide));
    t += L"\r\n[device]\r\n";
    put(L"device", s.device);
    flag(L"autoConnect", s.autoConnect);
    t += L"\r\n[models]\r\n";
    for (auto& m : s.models) put(m.first.c_str(), m.second);
    return writeFile(path, toUtf8(t));
}
} // namespace settings
