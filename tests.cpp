// Protocol, model table, settings and link tests (no UI). Built and run by build.ps1.
#include "bt.hpp"
#include "fakebuds.hpp"
#include "models.hpp"
#include "settings.hpp"
#include <cmath>
#include <cstdio>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static proto::Packet packetOf(uint16_t cmd, const proto::Bytes& payload) {
    proto::Framer f;
    proto::Bytes b = proto::build(cmd, payload, 7);
    f.push(b.data(), b.size());
    proto::Packet p;
    f.next(p);
    return p;
}

static void testCrcAndBuild() {
    // CRC-16/MODBUS check value.
    const char* nine = "123456789";
    CHECK(proto::crc16((const uint8_t*)nine, 9) == 0x4B37);
    // ear (web)'s battery request: 55 60 01 07 C0 00 00 <op> <crc lo> <crc hi>
    proto::Bytes b = proto::build(proto::GetBattery, {}, 1);
    CHECK(b.size() == 10);
    CHECK(b[0] == 0x55 && b[1] == 0x60 && b[2] == 0x01 && b[3] == 0x07 && b[4] == 0xC0 && b[5] == 0 && b[6] == 0 && b[7] == 1);
    uint16_t crc = proto::crc16(b.data(), 8);
    CHECK(b[8] == (crc & 0xFF) && b[9] == (crc >> 8));
    proto::Bytes anc = proto::build(proto::SetAnc, {1, 7, 0}, 9);
    CHECK(anc[3] == 0x0F && anc[4] == 0xF0 && anc[5] == 3 && anc[8] == 1 && anc[9] == 7 && anc[10] == 0);
}

static void testFramer() {
    proto::Bytes a = proto::build(proto::RspAnc, {1, 5}, 3), c = proto::build(proto::RspBass, {1, 6}, 4);
    // Garbage, two packets in one read, then a packet split across reads.
    proto::Bytes stream{0x01, 0x02};
    stream.insert(stream.end(), a.begin(), a.end());
    stream.insert(stream.end(), c.begin(), c.end());
    proto::Framer f;
    f.push(stream.data(), stream.size() - 3);
    proto::Packet p;
    CHECK(f.next(p) && p.command == proto::RspAnc && p.operation == 3 && p.at(9) == 5);
    CHECK(!f.next(p));
    f.push(stream.data() + stream.size() - 3, 3);
    CHECK(f.next(p) && p.command == proto::RspBass && p.at(9) == 6);
    CHECK(!f.next(p) && f.pending() == 0);   // both CRCs were recognised and consumed
    // No CRC flag, packets back to back.
    proto::Framer g;
    proto::Bytes h1{0x55, 0x60, 0x01, 0x03, 0xE0, 0x02, 0x00, 0x00, 0x01, 0x07}, both = h1;
    both.insert(both.end(), h1.begin(), h1.end());
    g.push(both.data(), both.size());
    CHECK(g.next(p) && p.command == proto::PushAnc && p.at(9) == 7);
    CHECK(g.next(p) && p.command == proto::PushAnc);
    CHECK(!g.next(p));
    // A CRC that arrives in a later read than its packet is still dropped.
    proto::Framer late;
    proto::Bytes full = proto::build(proto::RspAnc, {1, 3}, 1);
    late.push(full.data(), full.size() - 2);
    CHECK(late.next(p) && p.at(9) == 3);
    late.push(full.data() + full.size() - 2, 2);
    late.push(h1.data(), h1.size());
    CHECK(late.next(p) && p.command == proto::PushAnc && !late.next(p) && late.pending() == 0);
    // No CRC at all: the next packet starts right away.
    proto::Framer k;
    proto::Bytes n1{0x55, 0x60 | 0x20, 0x01, 0x03, 0xE0, 0x02, 0x00, 0x00, 0x01, 0x01}, two = n1;
    two.insert(two.end(), h1.begin(), h1.end());
    k.push(two.data(), two.size());
    CHECK(k.next(p) && p.at(9) == 1);
    CHECK(k.next(p) && p.at(9) == 7);
    // An absurd length is not a header: the framer skips it and resyncs.
    proto::Framer r;
    proto::Bytes bad{0x55, 0x60, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0x00};
    bad.insert(bad.end(), h1.begin(), h1.end());
    r.push(bad.data(), bad.size());
    CHECK(r.next(p) && p.command == proto::PushAnc);
}

static void testApply() {
    proto::State s;
    CHECK(proto::apply(s, packetOf(proto::RspBattery, {3, 2, 0x80 | 64, 3, 70, 4, 100})) == proto::ChBattery);
    CHECK(s.left.level == 64 && s.left.charging && s.right.level == 70 && !s.right.charging && s.caseB.level == 100 && s.stereo.level == -1);
    proto::apply(s, packetOf(proto::PushBattery, {1, 6, 45}));
    CHECK(s.stereo.level == 45 && s.left.level == -1);
    proto::apply(s, packetOf(proto::RspAnc, {1, proto::AncTransparency}));
    CHECK(s.anc == proto::AncTransparency);
    proto::apply(s, packetOf(proto::PushAnc2, {1, proto::AncAdaptive}));
    CHECK(s.anc == proto::AncAdaptive && proto::ancIsNoiseCancelling(s.anc) && !proto::ancIsNoiseCancelling(proto::AncOff));
    proto::apply(s, packetOf(proto::RspListeningMode, {5}));
    CHECK(s.eqMode == 5);
    proto::apply(s, packetOf(proto::RspBass, {1, 8}));
    CHECK(s.bassOn == 1 && s.bassWire == 8);
    proto::apply(s, packetOf(proto::RspFirmware, {'1', '.', '0', '.', '2', '.', '1', '0', '3'}));
    CHECK(s.firmware == "1.0.2.103");
    proto::apply(s, packetOf(proto::RspInEar, {1, 1, 0}));
    CHECK(s.inEar == 0);
    proto::apply(s, packetOf(proto::RspLatency, {1}));
    CHECK(s.latency == 1);
    proto::apply(s, packetOf(proto::RspLatency, {2}));
    CHECK(s.latency == 0);
    proto::apply(s, packetOf(proto::RspGesture, {2, 2, 1, 2, 9, 3, 1, 7, 21}));
    CHECK(s.gestures.size() == 2 && s.gestures[0].device == 2 && s.gestures[0].action == 9 && s.gestures[1].type == 7 && s.gestures[1].action == 21);
    CHECK(proto::apply(s, packetOf(proto::PushEarFit, {0, 1})) == proto::ChFit && s.fitLeft == 0 && s.fitRight == 1);
    proto::apply(s, packetOf(proto::RspCodec, {2}));
    CHECK(s.codec == 2);
    proto::apply(s, packetOf(proto::RspSpatial, {1, 1}));
    CHECK(s.spatialMode == 1 && s.spatialHead == 1 && models::spatialIndex(1, 1) == 1 && models::spatialIndex(1, 0) == 2);
    proto::apply(s, packetOf(proto::RspAudiodo, {1}));
    CHECK(s.personal == 1);
    CHECK(proto::apply(s, packetOf(0x4999, {1})) == 0);
    // Truncated packets never read past the end.
    proto::State t;
    proto::apply(t, packetOf(proto::RspBattery, {5, 2}));
    CHECK(t.left.level == -1);
    CHECK(proto::apply(t, packetOf(proto::RspAnc, {})) == 0 && t.anc == 0);
}

static void testCustomEq() {
    proto::Bytes p = proto::customEqPayload(4, -3, 2, proto::EqShape{});
    CHECK(p.size() == 1 + 4 + 3 * 13 + 9);
    CHECK(p[0] == 3 && p[5] == 1 && p[18] == 2 && p[31] == 0);   // band types: peak, high shelf, low shelf
    float total; memcpy(&total, &p[1], 4);
    CHECK(total == -4.0f);
    // The device echoes the same layout; read it back.
    proto::State s;
    CHECK(proto::apply(s, packetOf(proto::RspCustomEq, p)) == proto::ChCustomEq);
    CHECK(s.custom[0] == 4 && s.custom[1] == -3 && s.custom[2] == 2);
    // Negative gains with ear (web)'s sign-in-the-last-byte encoding.
    uint8_t odd[4] = {0x80, 0, 0, 0};   // -0.0
    CHECK(proto::getFloat(odd) == 0.0f);
    float v = -5.0f; uint8_t le[4]; memcpy(le, &v, 4);
    CHECK(proto::getFloat(le) == -5.0f);
    // All gains <= 0: total gain is -0 (no boost to compensate).
    proto::Bytes z = proto::customEqPayload(-2, -2, -2, proto::EqShape{});
    memcpy(&total, &z[1], 4);
    CHECK(total == 0.0f);
}

static void testDual() {
    proto::State s;
    proto::Bytes list{2, 0, 2};
    auto add = [&](uint8_t flags, uint8_t base, const char* name) {
        list.push_back(flags);
        for (int k = 0; k < 6; ++k) list.push_back((uint8_t)(base + k));
        list.push_back((uint8_t)strlen(name));
        list.insert(list.end(), name, name + strlen(name));
    };
    add(0x11, 0x10, "PC");
    add(0x00, 0x20, "Phone");
    proto::apply(s, packetOf(proto::RspDualList, list));
    CHECK(s.dual.size() == 2 && s.dualListGrew && s.dualTotal == 2);
    CHECK(s.dual[0].self && s.dual[0].connected && s.dual[0].name == "PC" && s.dual[0].mac == "10-11-12-13-14-15");
    CHECK(!s.dual[1].self && !s.dual[1].connected && s.dual[1].name == "Phone");
    proto::apply(s, packetOf(proto::RspDualList, list));   // same page again: nothing new
    CHECK(s.dual.size() == 2 && !s.dualListGrew);
    proto::apply(s, packetOf(proto::PushDualEvent, {1, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0}));
    CHECK(s.dual[1].connected && !s.dualListStale);
    proto::apply(s, packetOf(proto::PushDualEvent, {1, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0}));
    CHECK(s.dualListStale);
    uint8_t mac[6];
    CHECK(proto::macBytes("20-21-22-23-24-25", mac) && mac[0] == 0x20 && mac[5] == 0x25);
    CHECK(proto::macBytes("aa:bb:cc:dd:ee:ff", mac) && mac[5] == 0xFF);
    CHECK(!proto::macBytes("20-21-22", mac) && !proto::macBytes("20-21-22-23-24-25-26", mac) && !proto::macBytes("zz-21-22-23-24-25", mac));
}

static void testModels() {
    auto fp = models::fastPair("5C587F");
    CHECK(fp && std::string(fp->base) == "B184");
    CHECK(models::fastPair("5c587f") == fp || models::fastPair("5c587f") == nullptr);
    CHECK(!models::fastPair("000000"));
    auto c = models::capsFor("B184");
    CHECK(c && c->listeningMode && c->bass && c->espeonGestures && c->fitTest && c->dual && c->personal == 2 && c->ancLevel == 63);
    CHECK(!models::capsFor("B999"));
    // Every Fast Pair entry points at a model the caps table knows.
    for (auto& f : models::FastPairIds) { bool known = models::capsFor(f.base) != nullptr; CHECK(known); if (!known) printf("  no caps for %s\n", f.base); }
    CHECK(models::guessFromName(L"CMF Buds 2 Plus") == c);
    CHECK(models::guessFromName(L"Nothing Ear (a)") != nullptr);
    CHECK(models::looksLikeNothing(L"CMF Buds Pro 2") && models::looksLikeNothing(L"Nothing Ear") && !models::looksLikeNothing(L"WH-1000XM5"));
    // Noise-control cycle codes round-trip.
    for (int a : {10, 20, 21, 22}) {
        bool t, n, o;
        models::noiseControlParts(a, t, n, o);
        CHECK(models::noiseControlAction(t, n, o) == a);
    }
    CHECK(models::noiseControlAction(true, false, false) == 0);   // a cycle needs two modes
    for (auto& slot : models::espeonSlots()) CHECK(!slot.actions.empty());
    CHECK(bt::parseAddr(L"AA:BB:CC:DD:EE:FF") == 0xAABBCCDDEEFFull && bt::addrString(0xAABBCCDDEEFFull) == L"AA:BB:CC:DD:EE:FF");
    CHECK(bt::parseAddr(L"AA:BB") == 0 && bt::parseAddr(L"hello") == 0);
    bt::Target t;
    CHECK(bt::parseFake("127.0.0.1:5000:4AEB6E", t) && t.fakePort == 5000 && t.fakeModel == "4AEB6E" && t.fake());
    CHECK(!bt::parseFake("nothing", t));
}

static void testSettings() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"BudsPlusTest\\sub\\settings.ini";
    settings::Settings s;
    s.theme = 3; s.haveWindow = true; s.window = RECT{10, 20, 500, 900};
    s.device = L"AA:BB:CC:DD:EE:FF"; s.autoConnect = false; s.tray = false; s.closeToTray = true; s.startMinimized = true;
    s.notifyLowBattery = false; s.gestureSide = 1; s.models[L"AA:BB:CC:DD:EE:FF"] = L"5C587F";
    CHECK(settings::save(path, s));
    settings::Settings r = settings::load(path);
    CHECK(r.theme == 3 && r.haveWindow && r.window.left == 10 && r.window.bottom == 900 && r.device == s.device);
    CHECK(!r.autoConnect && !r.tray && r.closeToTray && r.startMinimized && !r.notifyLowBattery && r.gestureSide == 1);
    CHECK(r.models.size() == 1 && r.models[L"AA:BB:CC:DD:EE:FF"] == L"5C587F");
    settings::Settings d = settings::load(path + L".missing");
    CHECK(d.theme == 0 && d.autoConnect && d.tray && !d.haveWindow);
    settings::writeFile(path, "\xEF\xBB\xBF[ui]\r\ntheme = 9\r\nwindow = 5,5,1,1\r\n");
    r = settings::load(path);
    CHECK(r.theme == 0 && !r.haveWindow);
    DeleteFileW(path.c_str());
    RemoveDirectoryW((std::wstring(tmp) + L"BudsPlusTest\\sub").c_str());
    RemoveDirectoryW((std::wstring(tmp) + L"BudsPlusTest").c_str());
}

// Replies captured from a real CMF Buds Pro 2 (BUDSPLUS_LOG). They carry no CRC, and some unsolicited
// ones use 0x20 as the second header byte. Device names in the dual list are replaced.
static void testRealCaptures() {
    proto::Bytes stream{
        0x55, 0x60, 0x01, 0x07, 0x40, 0x05, 0x00, 0x01, 0x02, 0x02, 0x46, 0x03, 0x41,                     // battery L70 R65
        0x55, 0x20, 0x01, 0x4F, 0x40, 0x01, 0x00, 0x00, 0x01,                                             // unsolicited spatial
        0x55, 0x60, 0x01, 0x1E, 0x40, 0x06, 0x00, 0x04, 0x01, 0x05, 0x00, 0x02, 0x04, 0x00,               // ANC off
        0x55, 0x60, 0x01, 0x44, 0x40, 0x2C, 0x00, 0x06, 0x03, 0x00, 0x00, 0x80, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x75, 0x44, 0x33, 0x33, 0x33, 0x3F, 0x02, 0x00, 0x00, 0x40, 0xC0, 0x00, 0xA0, 0xD7, 0x45, 0x00, 0x00, 0x80, 0x3F, 0x00,
        0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x0C, 0x43, 0xCD, 0xCC, 0x4C, 0x3F,                           // custom EQ +4 / 0 / -3
        0x55, 0x60, 0x01, 0x18, 0x40, 0x3D, 0x00, 0x09, 0x0F, 0x03, 0x01, 0x02, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x07,
        0x01, 0x03, 0x01, 0x09, 0x01, 0x02, 0x01, 0x02, 0x09, 0x02, 0x01, 0x03, 0x01, 0x02, 0x01, 0x07, 0x01, 0x02, 0x01, 0x09,
        0x01, 0x04, 0x01, 0x01, 0x01, 0x04, 0x01, 0x02, 0x01, 0x04, 0x01, 0x03, 0x01, 0x04, 0x01, 0x07, 0x01, 0x04, 0x01, 0x0A,
        0x01, 0x04, 0x09, 0x02, 0x01, 0x04, 0x09, 0x03, 0x01,                                             // 15 gestures (device 4 = case)
        0x55, 0x60, 0x01, 0x4C, 0x40, 0x00, 0x00, 0x0A,                                                   // advanced EQ, empty
        0x55, 0x60, 0x01, 0x28, 0x40, 0x0E, 0x00, 0x0E, 0x02, 0x01, 0x01, 0x11, 0x9A, 0x4B, 0xF8, 0xC8, 0x03, 0x30, 0x03, 'P', 'C', '1', // dual page 2 of 2
        0x55, 0x60, 0x01, 0x4F, 0x40, 0x01, 0x00, 0x11, 0x01,                                             // spatial
    };
    proto::Framer f;
    f.push(stream.data(), stream.size());
    proto::State s;
    proto::Packet p;
    int n = 0;
    while (f.next(p)) { proto::apply(s, p); ++n; }
    CHECK(n == 8 && f.pending() == 0);
    CHECK(s.left.level == 70 && s.right.level == 65 && s.caseB.level == -1);
    CHECK(s.anc == proto::AncOff);
    CHECK(s.custom[0] == 4 && s.custom[1] == 0 && s.custom[2] == -3);
    CHECK(s.gestures.size() == 15 && s.gestures[4].device == 2 && s.gestures[4].type == 2 && s.gestures[4].action == 9);
    CHECK(s.dual.size() == 1 && s.dualTotal == 2 && s.dual[0].self && s.dual[0].connected && s.dual[0].name == "PC1");
    CHECK(s.spatialMode == 1 && models::spatialIndex(s.spatialMode, s.spatialHead) == 2);
}

template <class F> static bool waitFor(F cond, int ms = 3000) {
    for (int i = 0; i < ms / 10; ++i) { if (cond()) return true; Sleep(10); }
    return cond();
}

static void testLink() {
    FakeBuds fake;
    CHECK(fake.start());
    bt::Link link;
    bt::Target t;
    bt::parseFake("127.0.0.1:" + std::to_string(fake.port) + ":5C587F", t);
    link.start(t);
    std::vector<bt::Event> got;
    auto pump = [&] { for (auto& e : link.drain()) got.push_back(e); };
    auto has = [&](bt::EventType type) { pump(); for (auto& e : got) if (e.type == type) return true; return false; };
    CHECK(waitFor([&] { return has(bt::EvConnected); }));
    CHECK(got.size() >= 2 && got[0].type == bt::EvModel && got[0].model == "5C587F" && got[0].generation == link.current());
    proto::State s;
    link.send(proto::build(proto::GetBattery, {}, 1));
    link.send(proto::build(proto::GetAnc, {}, 2));
    CHECK(waitFor([&] { pump(); for (auto& e : got) if (e.type == bt::EvPacket) proto::apply(s, e.packet); got.clear(); return s.anc != 0 && s.left.level >= 0; }));
    CHECK(s.left.level == 82 && s.right.level == 77 && s.caseB.level == 54 && s.anc == proto::AncHigh);
    link.send(proto::build(proto::SetAnc, {1, proto::AncOff, 0}, 3));
    CHECK(waitFor([&] { pump(); for (auto& e : got) if (e.type == bt::EvPacket) proto::apply(s, e.packet); got.clear(); return s.anc == proto::AncOff; }));
    { std::lock_guard<std::mutex> g(fake.m); CHECK(fake.anc == proto::AncOff && fake.lastSet == proto::SetAnc); }
    // The device going away is reported once, as EvClosed.
    fake.drop();
    CHECK(waitFor([&] { return has(bt::EvClosed); }));
    // A new connection gets a new generation; stop() leaves no worker behind.
    unsigned before = link.current();
    link.start(t);
    CHECK(link.current() == before + 1);
    got.clear();
    CHECK(waitFor([&] { return has(bt::EvConnected); }));
    link.stop();
    fake.stop();
    // Nothing listening: EvFailed with a message.
    bt::Link dead;
    bt::Target nowhere;
    bt::parseFake("127.0.0.1:" + std::to_string(fake.port) + ":5C587F", nowhere);
    dead.start(nowhere);
    got.clear();
    std::vector<bt::Event> d;
    CHECK(waitFor([&] { for (auto& e : dead.drain()) d.push_back(e); for (auto& e : d) if (e.type == bt::EvFailed) return !e.error.empty(); return false; }));
}

int wmain() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    testCrcAndBuild();
    testFramer();
    testApply();
    testCustomEq();
    testDual();
    testModels();
    testSettings();
    testRealCaptures();
    testLink();
    WSACleanup();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
