// Bluetooth side: paired-device enumeration and the RFCOMM link to the earbuds (namespace bt).
// ear (web) opens two RFCOMM services through Web Serial; here they are plain Winsock AF_BTH sockets,
// with Windows doing the SDP lookup from the service UUID:
//   Fast Pair (df21fe2c-...)  read once: its first message carries the 3-byte Fast Pair model ID;
//   control   (aeac4a03-...)  the command channel (see protocol.hpp).
// A "fake" target (host:port:modelId) uses TCP instead, for the tests.
#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2bth.h>
#include <windows.h>
#include <bluetoothapis.h>
#include "protocol.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace bt {

const GUID ControlService = {0xaeac4a03, 0xdff5, 0x498f, {0x84, 0x3a, 0x34, 0x48, 0x7c, 0xf1, 0x33, 0xeb}};
const GUID FastPairService = {0xdf21fe2c, 0x2515, 0x4fdb, {0x88, 0x86, 0xf1, 0x2c, 0x4d, 0x67, 0x92, 0x7c}};

struct Device {
    uint64_t addr = 0;
    std::wstring name;
    bool connected = false;
};

inline std::wstring addrString(uint64_t a) {
    wchar_t b[32];
    swprintf(b, 32, L"%02X:%02X:%02X:%02X:%02X:%02X", (unsigned)(a >> 40) & 255, (unsigned)(a >> 32) & 255, (unsigned)(a >> 24) & 255,
             (unsigned)(a >> 16) & 255, (unsigned)(a >> 8) & 255, (unsigned)a & 255);
    return b;
}
inline uint64_t parseAddr(const std::wstring& s) {
    uint64_t v = 0;
    int digits = 0;
    for (wchar_t c : s) {
        int d = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;
        if (d >= 0) { v = v * 16 + d; ++digits; }
        else if (c != L':' && c != L'-') return 0;
    }
    return digits == 12 ? v : 0;
}

// False when there is no adapter or Bluetooth is switched off (Windows then reports no radio).
inline bool radioOn() {
    BLUETOOTH_FIND_RADIO_PARAMS rp{};
    rp.dwSize = sizeof rp;
    HANDLE radio = nullptr;
    HBLUETOOTH_RADIO_FIND f = BluetoothFindFirstRadio(&rp, &radio);
    if (!f) return false;
    if (radio) CloseHandle(radio);
    BluetoothFindRadioClose(f);
    return true;
}

// Devices Windows knows (paired / remembered); no inquiry, so this returns at once.
inline std::vector<Device> pairedDevices() {
    std::vector<Device> out;
    BLUETOOTH_DEVICE_SEARCH_PARAMS p{};
    p.dwSize = sizeof p;
    p.fReturnAuthenticated = TRUE;
    p.fReturnRemembered = TRUE;
    p.fReturnConnected = TRUE;
    p.fReturnUnknown = FALSE;
    p.fIssueInquiry = FALSE;
    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof info;
    HBLUETOOTH_DEVICE_FIND f = BluetoothFindFirstDevice(&p, &info);
    if (!f) return out;
    do {
        Device d;
        d.addr = info.Address.ullLong;
        d.name = info.szName;
        d.connected = info.fConnected != FALSE;
        bool dup = false;
        for (auto& e : out) if (e.addr == d.addr) dup = true;
        if (!dup) out.push_back(d);
        info = BLUETOOTH_DEVICE_INFO{};
        info.dwSize = sizeof info;
    } while (BluetoothFindNextDevice(f, &info));
    BluetoothFindDeviceClose(f);
    return out;
}

inline std::wstring errorText(int code) {
    switch (code) {
    case WSAETIMEDOUT: return L"The device did not answer. Make sure it is on, out of the case and paired with this PC.";
    case WSAECONNREFUSED: return L"The device refused the connection.";
    case WSAEHOSTDOWN: case WSAEHOSTUNREACH: case WSAENETUNREACH: return L"The device is out of range or switched off.";
    case WSAEADDRNOTAVAIL: return L"This device does not offer the earbuds control service.";
    case WSAEACCES: return L"Windows denied access to the device.";
    case WSAENETDOWN: return L"Bluetooth is turned off or no adapter is available.";
    case WSAEAFNOSUPPORT: case WSAEPROTONOSUPPORT: return L"Bluetooth sockets are not available on this PC.";
    }
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, (DWORD)code, 0, (LPWSTR)&msg, 0, nullptr);
    std::wstring s = msg ? msg : L"";
    if (msg) LocalFree(msg);
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L'.')) s.pop_back();
    wchar_t code_[32]; swprintf(code_, 32, L" (error %d)", code);
    return (s.empty() ? std::wstring(L"Connection failed") : s) + code_;
}

struct Target {
    uint64_t addr = 0;           // Bluetooth address, or
    std::string fakeHost;        // a TCP stand-in: host, port, Fast Pair model ID
    int fakePort = 0;
    std::string fakeModel;
    bool fake() const { return fakePort != 0; }
};
// "host:port:MODEL" (BUDSPLUS_FAKE) -> Target
inline bool parseFake(const std::string& s, Target& t) {
    size_t a = s.find(':'), b = a == std::string::npos ? a : s.find(':', a + 1);
    if (a == std::string::npos) return false;
    t.fakeHost = s.substr(0, a);
    t.fakePort = atoi(s.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1).c_str());
    t.fakeModel = b == std::string::npos ? "5C587F" : s.substr(b + 1);
    return t.fakePort > 0;
}

enum EventType { EvModel, EvConnected, EvPacket, EvClosed, EvFailed };
struct Event {
    EventType type;
    unsigned generation;
    std::string model;          // EvModel: Fast Pair model ID ("" when the device did not say)
    proto::Packet packet;       // EvPacket
    std::wstring error;         // EvFailed / EvClosed
};

// One connection attempt and its session, run on a worker thread. Events queue up and the window gets
// a single notify message per batch (coalesced, like TDM's WM_ENGINE); the UI thread drains them.
class Link {
    std::mutex m;
    std::deque<Event> events;
    SOCKET sock = INVALID_SOCKET, probe = INVALID_SOCKET;
    std::thread worker;
    std::atomic<bool> stopping{false}, notified{false};
    HWND notifyWnd = nullptr;
    UINT notifyMsg = 0;
    unsigned generation = 0;

    void push(Event e) {
        {
            std::lock_guard<std::mutex> g(m);
            events.push_back(std::move(e));
        }
        if (!notified.exchange(true) && notifyWnd) PostMessageW(notifyWnd, notifyMsg, 0, 0);
    }
    static SOCKET openBt(int& err) {
        SOCKET s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (s == INVALID_SOCKET) { err = WSAGetLastError(); return s; }
        return s;
    }
    static bool connectBt(SOCKET s, uint64_t addr, const GUID& service, int& err) {
        SOCKADDR_BTH sa{};
        sa.addressFamily = AF_BTH;
        sa.btAddr = addr;
        sa.serviceClassId = service;
        sa.port = 0;
        if (connect(s, (sockaddr*)&sa, sizeof sa) == SOCKET_ERROR) { err = WSAGetLastError(); return false; }
        return true;
    }
    // Reads the Fast Pair message stream until the model-ID message (group 3, code 1) or a timeout.
    std::string readFastPairModel(uint64_t addr) {
        int err = 0;
        SOCKET s = openBt(err);
        if (s == INVALID_SOCKET) return "";
        { std::lock_guard<std::mutex> g(m); probe = s; }
        std::string model;
        if (!stopping && connectBt(s, addr, FastPairService, err)) {
            DWORD timeout = 2500;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
            proto::Bytes buf;
            uint8_t chunk[256];
            DWORD started = GetTickCount();
            while (!stopping && model.empty() && GetTickCount() - started < 3000) {
                int n = recv(s, (char*)chunk, sizeof chunk, 0);
                if (n <= 0) break;
                buf.insert(buf.end(), chunk, chunk + n);
                size_t i = 0;
                while (i + 4 <= buf.size()) {
                    size_t len = ((size_t)buf[i + 2] << 8) | buf[i + 3];
                    if (i + 4 + len > buf.size()) break;
                    if (buf[i] == 3 && buf[i + 1] == 1 && len == 3) {
                        char hex[8];
                        snprintf(hex, sizeof hex, "%02X%02X%02X", buf[i + 4], buf[i + 5], buf[i + 6]);
                        model = hex;
                        break;
                    }
                    i += 4 + len;
                }
                buf.erase(buf.begin(), buf.begin() + std::min(i, buf.size()));
            }
        }
        { std::lock_guard<std::mutex> g(m); probe = INVALID_SOCKET; }
        closesocket(s);
        return model;
    }
    void run(Target t, unsigned gen) {
        int err = 0;
        SOCKET s = INVALID_SOCKET;
        if (t.fake()) {
            push(Event{EvModel, gen, t.fakeModel, {}, L""});
            addrinfo hints{}, *res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(t.fakeHost.c_str(), std::to_string(t.fakePort).c_str(), &hints, &res) == 0 && res) {
                s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                { std::lock_guard<std::mutex> g(m); sock = s; }
                if (s != INVALID_SOCKET && connect(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) err = WSAGetLastError();
                freeaddrinfo(res);
            } else err = WSAHOST_NOT_FOUND;
            if (s == INVALID_SOCKET && !err) err = WSAGetLastError();
        } else {
            std::string model = readFastPairModel(t.addr);
            if (stopping) return;
            push(Event{EvModel, gen, model, {}, L""});
            s = openBt(err);
            { std::lock_guard<std::mutex> g(m); sock = s; }
            if (s != INVALID_SOCKET && !stopping) connectBt(s, t.addr, ControlService, err);
        }
        if (stopping) return;
        if (s == INVALID_SOCKET || err) {
            push(Event{EvFailed, gen, "", {}, errorText(err ? err : WSAGetLastError())});
            return;
        }
        push(Event{EvConnected, gen, "", {}, L""});
        proto::Framer framer;
        uint8_t chunk[2048];
        for (;;) {
            int n = recv(s, (char*)chunk, sizeof chunk, 0);
            if (n <= 0) {
                if (!stopping) push(Event{EvClosed, gen, "", {}, n == 0 ? L"The device closed the connection." : errorText(WSAGetLastError())});
                return;
            }
            framer.push(chunk, (size_t)n);
            proto::Packet pk;
            while (framer.next(pk)) push(Event{EvPacket, gen, "", pk, L""});
        }
    }

public:
    void setNotify(HWND h, UINT msg) { notifyWnd = h; notifyMsg = msg; }
    unsigned current() const { return generation; }
    void start(const Target& t) {
        stop();
        stopping = false;
        unsigned gen = ++generation;
        worker = std::thread([this, t, gen] { run(t, gen); });
    }
    void stop() {
        stopping = true;
        {
            std::lock_guard<std::mutex> g(m);
            if (probe != INVALID_SOCKET) { shutdown(probe, SD_BOTH); closesocket(probe); probe = INVALID_SOCKET; }
            if (sock != INVALID_SOCKET) { shutdown(sock, SD_BOTH); closesocket(sock); sock = INVALID_SOCKET; }
        }
        if (worker.joinable()) worker.join();
        {
            std::lock_guard<std::mutex> g(m);
            if (sock != INVALID_SOCKET) { closesocket(sock); sock = INVALID_SOCKET; }
        }
    }
    bool send(const proto::Bytes& b) {
        std::lock_guard<std::mutex> g(m);
        if (sock == INVALID_SOCKET) return false;
        size_t done = 0;
        while (done < b.size()) {
            int n = ::send(sock, (const char*)b.data() + done, (int)(b.size() - done), 0);
            if (n <= 0) return false;
            done += (size_t)n;
        }
        return true;
    }
    std::deque<Event> drain() {
        notified = false;
        std::lock_guard<std::mutex> g(m);
        std::deque<Event> out;
        out.swap(events);
        return out;
    }
    ~Link() { stop(); }
};
} // namespace bt
