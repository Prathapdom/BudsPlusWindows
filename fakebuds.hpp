// A stand-in for a pair of CMF Buds 2 Plus, for the tests: a TCP server that answers the control
// protocol the way the earbuds do (BudsPlus connects to it through BUDSPLUS_FAKE=127.0.0.1:port:model).
// It keeps the settings it is sent, so a test can check what the app wrote.
#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "protocol.hpp"
#include <atomic>
#include <mutex>
#include <thread>

struct FakeBuds {
    // Device state (guarded by m).
    std::mutex m;
    int left = 82, right = 77, caseLevel = 54;
    bool leftCharging = false;
    int anc = proto::AncHigh, listening = 3, bassOn = 1, bassWire = 4, inEar = 1, latency = 2, codec = 0, spatialMode = 0, spatialHead = 0;
    int personal = 0, dualEnabled = 1;
    proto::Bytes customEq = proto::customEqPayload(0, 0, 0, proto::EqShape{});
    proto::Gesture gestures[8] = {{2, 1, 2, 2}, {3, 1, 2, 2}, {2, 1, 3, 9}, {3, 1, 3, 9}, {2, 1, 7, 10}, {3, 1, 7, 10}, {2, 1, 9, 18}, {3, 1, 9, 19}};
    bool dualConnected[2] = {true, false};
    int ringLeft = -1, ringRight = -1;
    int fitTests = 0, timeSets = 0, connections = 0, setCommands = 0;
    uint16_t lastSet = 0;
    proto::Bytes lastPayload;
    bool rebootOnCodec = true;

    SOCKET listener = INVALID_SOCKET, client = INVALID_SOCKET;
    int port = 0;
    std::atomic<bool> quit{false};
    std::thread worker;

    bool start(int fixedPort = 0) {   // 0: any free port
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons((u_short)fixedPort);
        if (bind(listener, (sockaddr*)&a, sizeof a) || listen(listener, 4)) return false;
        int len = sizeof a;
        getsockname(listener, (sockaddr*)&a, &len);
        port = ntohs(a.sin_port);
        worker = std::thread([this] { serve(); });
        return true;
    }
    void stop() {
        quit = true;
        closesocket(listener);
        { std::lock_guard<std::mutex> g(m); if (client != INVALID_SOCKET) shutdown(client, SD_BOTH); }
        if (worker.joinable()) worker.join();
    }
    // Drops the current connection, as the earbuds do when they reboot or go back in the case.
    void drop() { std::lock_guard<std::mutex> g(m); if (client != INVALID_SOCKET) shutdown(client, SD_BOTH); }

    void sendPacket(uint16_t cmd, const proto::Bytes& payload, uint8_t op = 0) {
        proto::Bytes b = proto::build(cmd, payload, op);
        SOCKET s;
        { std::lock_guard<std::mutex> g(m); s = client; }
        if (s != INVALID_SOCKET) ::send(s, (const char*)b.data(), (int)b.size(), 0);
    }
    void pushBattery() {
        proto::Bytes p;
        { std::lock_guard<std::mutex> g(m); p = {3, 2, (uint8_t)(left | (leftCharging ? 0x80 : 0)), 3, (uint8_t)right, 4, (uint8_t)caseLevel}; }
        sendPacket(proto::PushBattery, p);
    }

private:
    void serve() {
        while (!quit) {
            SOCKET s = accept(listener, nullptr, nullptr);
            if (s == INVALID_SOCKET) return;
            { std::lock_guard<std::mutex> g(m); client = s; ++connections; }
            proto::Framer f;
            uint8_t buf[2048];
            for (;;) {
                int n = recv(s, (char*)buf, sizeof buf, 0);
                if (n <= 0) break;
                f.push(buf, n);
                proto::Packet pk;
                while (f.next(pk)) handle(pk);
            }
            { std::lock_guard<std::mutex> g(m); client = INVALID_SOCKET; }
            closesocket(s);
        }
    }
    void handle(const proto::Packet& pk) {
        using namespace proto;
        uint16_t reply = (uint16_t)((pk.command & 0x0FFF) | 0x4000);
        Bytes payload(pk.raw.begin() + 8, pk.raw.end());
        Bytes out;
        bool reboot = false, fit = false;
        {
            std::lock_guard<std::mutex> g(m);
            if ((pk.command & 0xF000) == 0xF000) { ++setCommands; lastSet = pk.command; lastPayload = payload; }
            auto at = [&](size_t i) { return i < payload.size() ? payload[i] : (uint8_t)0; };
            switch (pk.command) {
            case GetBattery: out = {3, 2, (uint8_t)(left | (leftCharging ? 0x80 : 0)), 3, (uint8_t)right, 4, (uint8_t)caseLevel}; break;
            case GetFirmware: { const char* v = "1.0.2.103"; out.assign(v, v + strlen(v)); break; }
            case GetAnc: out = {1, (uint8_t)anc}; break;
            case GetListeningMode: out = {(uint8_t)listening}; break;
            case GetCustomEq: out = customEq; break;
            case GetInEar: out = {1, 1, (uint8_t)inEar}; break;
            case GetLatency: out = {(uint8_t)latency}; break;
            case GetGesture:
                out = {8};
                for (auto& x : gestures) { out.push_back(x.device); out.push_back(x.common); out.push_back(x.type); out.push_back(x.action); }
                break;
            case GetAdvancedEq: out = {0}; break;
            case GetBass: out = {(uint8_t)bassOn, (uint8_t)bassWire}; break;
            case GetDualEnable: out = {(uint8_t)dualEnabled}; break;
            case GetDualList: {
                // Total, page offset, devices in the page. Like the real earbuds, one device per page.
                int i0 = at(0) < 2 ? at(0) : 1;
                out = {2, (uint8_t)i0, 1};
                const char* names[2] = {"This PC", "Pixel 9"};
                for (int i = i0; i < i0 + 1; ++i) {
                    out.push_back((uint8_t)((i == 0 ? 0x10 : 0) | (dualConnected[i] ? 1 : 0)));
                    for (int k = 0; k < 6; ++k) out.push_back((uint8_t)(0xA0 + i * 16 + k));
                    out.push_back((uint8_t)strlen(names[i]));
                    out.insert(out.end(), names[i], names[i] + strlen(names[i]));
                }
                break;
            }
            case GetCodec: out = {(uint8_t)codec}; break;
            case GetSpatial: out = {(uint8_t)spatialMode, (uint8_t)spatialHead}; break;
            case GetAudiodo: out = {(uint8_t)personal}; break;

            case SetTime: ++timeSets; break;
            case SetAnc: anc = at(1); break;
            case SetListeningMode: listening = at(0); break;
            case SetCustomEq: customEq = payload; break;
            case SetInEar: inEar = at(2); break;
            case SetLatency: latency = at(0); break;
            case SetBass: bassOn = at(0); bassWire = at(1); break;
            case SetSpatial: spatialMode = at(0); spatialHead = at(1); break;
            case SetAudiodo: personal = at(0); break;
            case SetDualEnable: dualEnabled = at(0); break;
            case SetDualConnect: if (at(1) == 0xB0) dualConnected[1] = at(0) == 1; break;
            case SetCodec: codec = at(0); reboot = rebootOnCodec; break;
            case Ring: if (at(0) == 2) ringLeft = at(1); else if (at(0) == 3) ringRight = at(1); break;
            case EarFitTest: ++fitTests; fit = true; break;
            case SetGesture:
                for (auto& x : gestures) if (x.device == at(1) && x.type == at(3)) x.action = at(4);
                break;
            }
        }
        if (!out.empty() || (pk.command & 0xF000) == 0xF000) sendPacket(reply, out, pk.operation);
        if (pk.command == SetAnc) { std::lock_guard<std::mutex> g(m); out = {1, (uint8_t)anc}; }
        if (pk.command == SetAnc) sendPacket(PushAnc, out);
        if (pk.command == SetDualConnect) {
            Bytes ev{(uint8_t)(payload.size() > 0 ? payload[0] : 0)};
            ev.insert(ev.end(), payload.begin() + 1, payload.end());
            ev.push_back(0);
            sendPacket(PushDualEvent, ev);
        }
        if (fit) sendPacket(PushEarFit, {0, 1});
        if (reboot) { Sleep(100); drop(); }
    }
};
