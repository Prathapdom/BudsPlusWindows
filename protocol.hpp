// The Nothing / CMF earbuds control protocol, as spoken by ear (web) over Bluetooth RFCOMM.
// Pure data handling (no I/O), so tests.cpp can exercise it directly.
//
// Packet: 55 60 01 | command (u16 LE) | payload length (u16 LE) | operation id | payload | CRC-16/MODBUS (LE)
// Requests are 0xC0xx, writes 0xF0xx, replies 0x40xx, unsolicited pushes 0xE0xx / 0x60xx.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace proto {
using Bytes = std::vector<uint8_t>;

enum Command : uint16_t {
    GetBattery = 0xC007, GetInEar = 0xC00E, GetGesture = 0xC018, GetAnc = 0xC01E, GetEq = 0xC01F, GetMimi = 0xC022,
    GetDualEnable = 0xC027, GetDualList = 0xC028, GetCodec = 0xC029, GetLatency = 0xC041, GetFirmware = 0xC042,
    GetCustomEq = 0xC044, GetAdvancedEq = 0xC04C, GetBass = 0xC04E, GetSpatial = 0xC04F, GetListeningMode = 0xC050,
    GetAudiodo = 0xC05A,

    Ring = 0xF002, SetGesture = 0xF003, SetInEar = 0xF004, SetTime = 0xF00A, SetAnc = 0xF00F, SetEq = 0xF010,
    EarFitTest = 0xF014, SetMimi = 0xF015, SetDualEnable = 0xF01A, SetDualConnect = 0xF01B, SetCodec = 0xF01C,
    SetListeningMode = 0xF01D, SetLatency = 0xF040, SetCustomEq = 0xF041, SetAdvancedEq = 0xF04F, SetBass = 0xF051,
    SetSpatial = 0xF052, SetAudiodo = 0xF05C,

    RspBattery = 0x4007, RspInEar = 0x400E, RspGesture = 0x4018, RspAnc = 0x401E, RspEq = 0x401F, RspMimi = 0x4022,
    RspDualEnable = 0x4027, RspDualList = 0x4028, RspCodec = 0x4029, RspLatency = 0x4041, RspFirmware = 0x4042,
    RspCustomEq = 0x4044, RspAdvancedEq = 0x404C, RspBass = 0x404E, RspSpatial = 0x404F, RspListeningMode = 0x4050,
    RspAudiodo = 0x405A,

    PushBattery = 0xE001, PushAnc = 0xE003, PushEarFit = 0xE00D, PushDualEvent = 0xE00E, PushAudiodo = 0xE019,
    PushAnc2 = 0x6003, PushAudiodo2 = 0x6019,
};

// ANC wire values (byte 9 of an ANC reply, byte 1 of the SetAnc payload).
enum Anc : int { AncUnknown = 0, AncHigh = 1, AncMid = 2, AncLow = 3, AncAdaptive = 4, AncOff = 5, AncTransparency = 7 };
inline bool ancIsNoiseCancelling(int a) { return a >= AncHigh && a <= AncAdaptive; }

inline uint16_t crc16(const uint8_t* p, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int j = 0; j < 8; ++j) crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}

inline Bytes build(uint16_t command, const Bytes& payload, uint8_t operation) {
    Bytes b{0x55, 0x60, 0x01, (uint8_t)(command & 0xFF), (uint8_t)(command >> 8), (uint8_t)(payload.size() & 0xFF), (uint8_t)(payload.size() >> 8), operation};
    b.insert(b.end(), payload.begin(), payload.end());
    uint16_t crc = crc16(b.data(), b.size());
    b.push_back((uint8_t)(crc & 0xFF));
    b.push_back((uint8_t)(crc >> 8));
    return b;
}

struct Packet {
    uint16_t command = 0;
    uint8_t operation = 0;
    Bytes raw;      // the whole packet, header included: offsets below match ear (web)'s
    uint8_t at(size_t i) const { return i < raw.size() ? raw[i] : 0; }
    size_t size() const { return raw.size(); }
};

// Splits the RFCOMM byte stream into packets. RFCOMM keeps no message boundaries, so a read can
// hold half a packet or several; ear (web) assumes one per read, which is usually but not always true.
// Bytes that cannot start a packet are skipped, so the reader resyncs after noise.
// The trailing CRC is optional on what the earbuds send (ear (web) never reads it), so it is taken when the
// two bytes after a packet are its CRC, including when they arrive in a later read.
class Framer {
    Bytes buf;
    bool crcMaybeLate = false;
    uint16_t lateCrc = 0;
public:
    void push(const uint8_t* p, size_t n) { buf.insert(buf.end(), p, p + n); }
    bool next(Packet& out) {
        for (;;) {
            if (crcMaybeLate && !buf.empty()) {
                if (buf[0] == (uint8_t)(lateCrc & 0xFF)) {
                    if (buf.size() < 2) return false;
                    if (buf[1] == (uint8_t)(lateCrc >> 8)) buf.erase(buf.begin(), buf.begin() + 2);
                }
                crcMaybeLate = false;
            }
            size_t start = 0;
            while (start < buf.size() && buf[start] != 0x55) ++start;
            if (start) buf.erase(buf.begin(), buf.begin() + start);
            if (buf.size() < 8) return false;
            size_t len = buf[5] | (buf[6] << 8);
            if (len > 4096) { buf.erase(buf.begin()); continue; }   // not a header after all: resync
            size_t total = 8 + len;
            if (buf.size() < total) return false;
            size_t used = total;
            uint16_t want = crc16(buf.data(), total);
            if (buf.size() >= total + 2) {
                if (buf[total] == (uint8_t)(want & 0xFF) && buf[total + 1] == (uint8_t)(want >> 8)) used = total + 2;
            } else if (buf.size() == total + 1) {
                if (buf[total] == (uint8_t)(want & 0xFF)) return false;   // probably half a CRC: wait for the rest
            } else {
                crcMaybeLate = true;
                lateCrc = want;
            }
            out.command = (uint16_t)(buf[3] | (buf[4] << 8));
            out.operation = buf[7];
            out.raw.assign(buf.begin(), buf.begin() + total);
            buf.erase(buf.begin(), buf.begin() + used);
            return true;
        }
    }
    size_t pending() const { return buf.size(); }
};

// ---------------------------------------------------------------- EQ floats

inline void putFloat(Bytes& b, float f) { uint8_t t[4]; memcpy(t, &f, 4); b.insert(b.end(), t, t + 4); }   // little-endian
// ear (web)'s fromFormatFloatForEQ: little-endian IEEE-754, except that a value whose three high-order
// bytes are zero carries its sign in the top bit of the last byte.
inline float getFloat(const uint8_t* p) {
    uint8_t be[4] = {p[3], p[2], p[1], p[0]};
    bool negate = false;
    if (be[0] == 0 && be[1] == 0 && be[2] == 0 && (be[3] & 0x80)) { be[3] &= 0x7F; negate = true; }
    uint32_t u = ((uint32_t)be[0] << 24) | ((uint32_t)be[1] << 16) | ((uint32_t)be[2] << 8) | be[3];
    float f; memcpy(&f, &u, 4);
    return negate ? -f : f;
}

struct EqShape { float freqLow = 140, qLow = 0.8f, freqPeak = 980, qPeak = 0.7f, freqHigh = 6900, qHigh = 1.0f; };

// Custom 3-band EQ (bass, mid, treble in dB, -6..+6). Wire order is peak (mid), high shelf (treble), low shelf (bass).
inline Bytes customEqPayload(int bass, int mid, int treble, const EqShape& s) {
    struct Band { uint8_t type; float gain, freq, q; };
    Band bands[3] = {{1, (float)mid, s.freqPeak, s.qPeak}, {2, (float)treble, s.freqHigh, s.qHigh}, {0, (float)bass, s.freqLow, s.qLow}};
    float maxGain = 0;
    for (auto& b : bands) if (b.gain > maxGain) maxGain = b.gain;
    Bytes p{3};
    putFloat(p, -maxGain);
    for (auto& b : bands) { p.push_back(b.type); putFloat(p, b.gain); putFloat(p, b.freq); putFloat(p, b.q); }
    p.insert(p.end(), 9, 0);   // three zero bytes per band, as ear (web) sends
    return p;
}

// ---------------------------------------------------------------- device state

struct Battery { int level = -1; bool charging = false; };
struct Gesture { uint8_t device = 0, common = 0, type = 0, action = 0; };
struct DualDevice { std::string mac; std::string name; bool self = false, connected = false; };

enum Changed : unsigned {
    ChBattery = 1, ChAnc = 2, ChEq = 4, ChBass = 8, ChFirmware = 16, ChInEar = 32, ChLatency = 64, ChGesture = 128,
    ChFit = 256, ChDual = 512, ChCodec = 1024, ChSpatial = 2048, ChPersonal = 4096, ChCustomEq = 8192, ChAdvancedEq = 16384,
};

struct State {
    Battery left, right, caseB, stereo;
    int anc = AncUnknown;
    int eqMode = -1;               // listening mode (CMF) or EQ preset (Nothing)
    int custom[3] = {0, 0, 0};     // bass, mid, treble
    bool haveCustom = false;
    int advancedEq = -1;
    int bassOn = -1, bassWire = 0; // ultra bass enabled and its raw level byte
    std::string firmware;
    int inEar = -1, latency = -1;  // latency: 1 low-latency on, 0 off
    std::vector<Gesture> gestures;
    int fitLeft = -1, fitRight = -1;   // 0 good seal, 1 adjust, 2 not in ear
    int dualEnabled = -1;
    std::vector<DualDevice> dual;
    int dualTotal = 0;             // devices in the dual-connection list, as the last page said
    bool dualListGrew = false;     // a page of the dual-connection list added devices: ask for the next page
    bool dualListStale = false;    // an event named an unknown device: re-read the list
    int codec = -1;
    int spatialMode = -1, spatialHead = 0;
    int personal = -1;
};

inline std::string macString(const uint8_t* p) {
    static const char* hex = "0123456789ABCDEF";
    std::string s;
    for (int i = 0; i < 6; ++i) { if (i) s += '-'; s += hex[p[i] >> 4]; s += hex[p[i] & 15]; }
    return s;
}
inline bool macBytes(const std::string& s, uint8_t out[6]) {
    int n = 0, v = 0, digits = 0;
    for (char c : s) {
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d >= 0) { v = v * 16 + d; if (++digits == 2) { if (n >= 6) return false; out[n++] = (uint8_t)v; v = 0; digits = 0; } }
        else if (c != '-' && c != ':') return false;
    }
    return n == 6 && digits == 0;
}

// Applies one received packet; returns which parts of the state changed.
inline unsigned apply(State& s, const Packet& pk) {
    const Bytes& r = pk.raw;
    switch (pk.command) {
    case RspBattery: case PushBattery: {
        Battery* slots[7] = {nullptr, nullptr, &s.left, &s.right, &s.caseB, nullptr, &s.stereo};
        s.left = s.right = s.caseB = s.stereo = Battery{};
        int count = pk.at(8);
        for (int i = 0; i < count && 10 + i * 2 < (int)r.size(); ++i) {
            int id = r[9 + i * 2], v = r[10 + i * 2];
            if (id < 7 && slots[id]) { slots[id]->level = v & 0x7F; slots[id]->charging = (v & 0x80) != 0; }
        }
        return ChBattery;
    }
    case RspAnc: case PushAnc: case PushAnc2:
        if (r.size() > 9) { s.anc = r[9]; return ChAnc; }
        return 0;
    case RspEq: case RspListeningMode:
        if (r.size() > 8) { s.eqMode = r[8]; return ChEq; }
        return 0;
    case RspCustomEq:
        if (r.size() >= 14 + 2 * 13 + 4) {
            float g[3];
            for (int i = 0; i < 3; ++i) g[i] = getFloat(&r[14 + i * 13]);
            int v[3] = {(int)(g[2] + (g[2] < 0 ? -0.5f : 0.5f)), (int)(g[0] + (g[0] < 0 ? -0.5f : 0.5f)), (int)(g[1] + (g[1] < 0 ? -0.5f : 0.5f))};
            for (int i = 0; i < 3; ++i) s.custom[i] = v[i] < -6 ? -6 : v[i] > 6 ? 6 : v[i];
            s.haveCustom = true;
            return ChCustomEq;
        }
        return 0;
    case RspAdvancedEq:
        if (r.size() > 8) { s.advancedEq = r[8]; return ChAdvancedEq; }
        return 0;
    case RspBass:
        if (r.size() > 9) { s.bassOn = r[8]; s.bassWire = r[9]; return ChBass; }
        return 0;
    case RspFirmware: {
        size_t n = pk.at(5);
        s.firmware.clear();
        for (size_t i = 0; i < n && 8 + i < r.size(); ++i) if (r[8 + i] >= 32 && r[8 + i] < 127) s.firmware += (char)r[8 + i];
        return ChFirmware;
    }
    case RspInEar:
        if (r.size() > 10) { s.inEar = r[10]; return ChInEar; }
        return 0;
    case RspLatency:
        if (r.size() > 8) { s.latency = r[8] == 1 ? 1 : 0; return ChLatency; }
        return 0;
    case RspGesture: {
        s.gestures.clear();
        int count = pk.at(8);
        for (int i = 0; i < count && 12 + i * 4 < (int)r.size(); ++i)
            s.gestures.push_back(Gesture{r[9 + i * 4], r[10 + i * 4], r[11 + i * 4], r[12 + i * 4]});
        return ChGesture;
    }
    case PushEarFit:
        if (r.size() > 9) { s.fitLeft = r[8]; s.fitRight = r[9]; return ChFit; }
        return 0;
    case RspDualEnable:
        if (r.size() > 8) { s.dualEnabled = r[8] == 1; return ChDual; }
        return 0;
    case RspDualList: {
        s.dualListGrew = false;
        if (r.size() < 11) return 0;
        // Payload: total, offset of this page, devices in this page, then the devices.
        s.dualTotal = r[8];
        int count = r[10];
        size_t off = 11;
        for (int i = 0; i < count && off + 8 <= r.size(); ++i) {
            DualDevice d;
            d.self = (r[off] & 0xF0) != 0;
            d.connected = (r[off] & 0x0F) != 0;
            d.mac = macString(&r[off + 1]);
            size_t nameLen = r[off + 7] & 0x7F;
            if (off + 8 + nameLen > r.size()) nameLen = r.size() - off - 8;
            d.name.assign((const char*)&r[off + 8], nameLen);
            bool found = false;
            for (auto& e : s.dual) if (e.mac == d.mac) { e = d; found = true; }
            if (!found) { s.dual.push_back(d); s.dualListGrew = true; }
            off += 8 + nameLen;
        }
        return ChDual;
    }
    case PushDualEvent: {
        if (r.size() < 16) return 0;
        std::string mac = macString(&r[9]);
        for (auto& e : s.dual) if (e.mac == mac) { e.connected = r[8] == 1; return ChDual; }
        s.dualListStale = true;
        return ChDual;
    }
    case RspCodec:
        s.codec = r.size() > 8 && r[8] <= 2 ? r[8] : 0;
        return ChCodec;
    case RspSpatial:
        s.spatialMode = pk.at(8); s.spatialHead = pk.at(9);
        return ChSpatial;
    case RspMimi: case RspAudiodo: case PushAudiodo: case PushAudiodo2:
        s.personal = r.size() > 8 && r[8] == 1;
        return ChPersonal;
    }
    return 0;
}
} // namespace proto
