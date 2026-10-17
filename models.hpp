// Supported Nothing / CMF products: the Fast Pair model IDs ear (web) recognises, and what each model
// base supports. Capabilities come from ear (web)'s js/ear_config_file.json (newest firmware config)
// and the model checks in js/bluetooth_socket.js.
#pragma once
#include "protocol.hpp"
#include <cwchar>

namespace models {

struct FastPair { const char* id; const wchar_t* name; const char* base; const wchar_t* colour; };
const FastPair FastPairIds[] = {
    {"31D53D", L"Nothing Ear (1)", "B181", L"White"}, {"624011", L"Nothing Ear (1)", "B181", L"Black"},
    {"1016DD", L"Nothing Ear (stick)", "B157", L"White"},
    {"ACC520", L"Nothing Ear (2)", "B155", L"White"}, {"DEE8C0", L"Nothing Ear (2)", "B155", L"Black"},
    {"FEB1C7", L"Nothing Ear", "B171", L"White"}, {"A20444", L"Nothing Ear", "B171", L"Black"},
    {"5E3FBC", L"Nothing Ear (a)", "B162", L"White"}, {"03464E", L"Nothing Ear (a)", "B162", L"Black"}, {"8B6380", L"Nothing Ear (a)", "B162", L"Yellow"},
    {"C34F3B", L"Nothing Ear (a)", "B162", L"Black"}, {"404D6D", L"Nothing Ear (a)", "B162", L"White"}, {"839E9A", L"Nothing Ear (a)", "B162", L"Yellow"},
    {"FC3AAF", L"Nothing Ear (open)", "B174", L"White"}, {"CC3444", L"Nothing Ear (open)", "B174", L"Blue"},
    {"5F8F82", L"CMF Buds Pro", "B163", L"Orange"}, {"2EB1CA", L"CMF Buds Pro", "B163", L"White"}, {"ADD2C4", L"CMF Buds Pro", "B163", L"Black"},
    {"4DFC4A", L"CMF Neckband Pro", "B164", L"Orange"}, {"26C190", L"CMF Neckband Pro", "B164", L"White"}, {"AE35FD", L"CMF Neckband Pro", "B164", L"Black"},
    {"D35E18", L"CMF Buds", "B168", L"Orange"}, {"ACCE54", L"CMF Buds", "B168", L"White"}, {"150A27", L"CMF Buds", "B168", L"Black"},
    {"F29566", L"CMF Buds Pro 2", "B172", L"Black"}, {"CA36A6", L"CMF Buds Pro 2", "B172", L"White"}, {"A7B220", L"CMF Buds Pro 2", "B172", L"Orange"},
    {"2B353E", L"CMF Buds Pro 2", "B172", L"Blue"}, {"2F45F5", L"CMF Buds Pro 2", "B172", L"Black"}, {"E1BE45", L"CMF Buds Pro 2", "B172", L"White"},
    {"1253C0", L"CMF Buds Pro 2", "B172", L"Orange"}, {"0F1A4F", L"CMF Buds Pro 2", "B172", L"Blue"},
    {"F51D3D", L"CMF Buds Pro 2", "B195", L"Black"}, {"A544CE", L"CMF Buds Pro 2", "B195", L"White"}, {"9EAC04", L"CMF Buds Pro 2", "B195", L"Orange"},
    {"1D07D4", L"CMF Buds Pro 2", "B195", L"Blue"}, {"53BFE6", L"CMF Buds Pro 2", "B195", L"Green"}, {"072714", L"CMF Buds Pro 2", "B195", L"Pink"},
    {"19EF24", L"CMF Buds 2", "B179", L"Black"}, {"FF2AB0", L"CMF Buds 2", "B179", L"Green"}, {"D9AB5D", L"CMF Buds 2", "B179", L"Orange"},
    {"3B1320", L"CMF Buds 2", "B197", L"Black"}, {"359A7D", L"CMF Buds 2", "B197", L"Green"}, {"00FDED", L"CMF Buds 2", "B197", L"Orange"},
    {"5C587F", L"CMF Buds 2 Plus", "B184", L"Blue"}, {"4AEB6E", L"CMF Buds 2 Plus", "B184", L"White"},
    {"70F8E3", L"CMF Buds 2a", "B185", L"Black"}, {"ED5412", L"CMF Buds 2a", "B185", L"White"}, {"509CAE", L"CMF Buds 2a", "B185", L"Orange"},
    {"C19ECD", L"Nothing Headphone (1)", "B170", L"Black"}, {"2D6FDA", L"Nothing Headphone (1)", "B170", L"Grey"},
    {"7D46E5", L"Nothing Ear (3)", "B173", L"Black"}, {"C1EBFD", L"Nothing Ear (3)", "B173", L"White"},
    {"563DA5", L"CMF Headphone Pro", "B175", L"Green"}, {"73C9EB", L"CMF Headphone Pro", "B175", L"White"}, {"1EFB39", L"CMF Headphone Pro", "B175", L"Black"},
    {"BFD53B", L"Nothing Headphone (a)", "B186", L"Black"}, {"DE8953", L"Nothing Headphone (a)", "B186", L"White"}, {"97EF75", L"Nothing Headphone (a)", "B186", L"Yellow"},
    {"98D02B", L"Nothing Headphone (a)", "B186", L"Pink"}, {"6F6C71", L"Nothing Headphone (a)", "B186", L"Black"}, {"810478", L"Nothing Headphone (a)", "B186", L"White"},
    {"79B3A9", L"Nothing Headphone (a)", "B186", L"Yellow"}, {"A292C6", L"Nothing Headphone (a)", "B186", L"Pink"},
    {"7B2328", L"Nothing Ear (3a)", "B190", L"White"}, {"E9C3BE", L"Nothing Ear (3a)", "B190", L"Black"}, {"148887", L"Nothing Ear (3a)", "B190", L"Pink"},
    {"DB45D3", L"Nothing Ear (3a)", "B190", L"Yellow"},
    {"8F28ED", L"CMF Clip Pro", "B189", L"Blue"}, {"E6673A", L"CMF Clip Pro", "B189", L"Orange"}, {"DCD2CB", L"CMF Clip Pro", "B189", L"White"},
    {"DA1280", L"CMF Clip Pro", "B189", L"Black"},
    {"CDFDEC", L"Nothing Headphone (1) Pro", "B192", L"Black"}, {"086485", L"Nothing Headphone (1) Pro", "B192", L"Silver"},
};

enum Kind { Earbuds, Headphones, Neckband };

struct Caps {
    const char* base;
    const wchar_t* name;
    Kind kind;
    int ancLevel;              // bit mask, -1 = no ANC: 0x10 transparency, 0x04 high/low, 0x02 mid, 0x08 adaptive
    bool listeningMode;        // EQ presets through SetListeningMode (CMF) rather than SetEq
    bool bass;                 // Ultra Bass / Bass Enhance
    bool threeStageBass;       // bass levels 0-2 (wire 0, 5, 10) instead of 1-5 (wire 2-10)
    bool inEar, fitTest;
    bool dual, dualReboot;
    int codecs;                // bit mask: 2 LHDC, 4 LDAC (AAC is always there)
    int spatial;               // bit mask: 1 head tracked, 2 fixed, 8 concert, 16 theatre, 32 game
    int personal;              // 0 none, 1 Mimi, 2 Audiodo
    int dirac;                 // Dirac Opteo support tier (0 = none)
    bool espeonGestures;       // the CMF Buds Pro 2 / Buds 2 Plus gesture table (see espeon.js)
    proto::EqShape eq;
};

// The default EQ shape: shelves at 140 Hz and 6.9 kHz, peak at 980 Hz.
const proto::EqShape Shape69{};
const proto::EqShape Shape34{140, 0.8f, 980, 0.7f, 3400, 1.0f};
const proto::EqShape Shape35{140, 0.8f, 980, 0.7f, 3500, 1.0f};

const Caps AllCaps[] = {
    //  base    name                         kind        anc  lm     bass   3st    inEar  fit    dual   reboot codec spat pers dirac espeon eq
    {"B181", L"Nothing Ear (1)",            Earbuds,    53, false, false, false, true,  false, false, false, 0, 0,  0, 0, false, Shape34},
    {"B157", L"Nothing Ear (stick)",        Earbuds,    33, false, false, false, true,  false, false, false, 0, 0,  0, 0, false, {140, 0.8f, 980, 0.66f, 3500, 1.0f}},
    {"B155", L"Nothing Ear (2)",            Earbuds,    63, false, false, false, true,  true,  true,  true,  2, 0,  1, 0, false, Shape34},
    {"B163", L"CMF Buds Pro",               Earbuds,    55, false, false, false, true,  false, false, false, 0, 0,  0, 0, false, Shape34},
    {"B171", L"Nothing Ear",                Earbuds,    63, false, true,  false, true,  true,  true,  false, 6, 0,  1, 0, false, Shape34},
    {"B172", L"CMF Buds Pro 2",             Earbuds,    63, true,  true,  false, true,  true,  true,  true,  4, 6,  0, 1, true,  Shape69},
    {"B195", L"CMF Buds Pro 2",             Earbuds,    63, true,  true,  true,  true,  true,  true,  false, 4, 6,  0, 2, true,  Shape69},
    {"B168", L"CMF Buds",                   Earbuds,    49, true,  true,  false, true,  false, true,  true,  0, 0,  0, 1, false, Shape69},
    {"B174", L"Nothing Ear (open)",         Earbuds,    -1, false, false, false, false, false, true,  false, 0, 0,  0, 0, false, Shape34},
    {"B162", L"Nothing Ear (a)",            Earbuds,    63, false, true,  false, true,  true,  true,  false, 4, 0,  0, 0, false, Shape34},
    {"B184", L"CMF Buds 2 Plus",            Earbuds,    63, true,  true,  false, true,  true,  true,  false, 4, 6,  2, 2, true,  Shape69},
    {"B179", L"CMF Buds 2",                 Earbuds,    63, true,  true,  false, true,  true,  true,  false, 0, 6,  0, 1, false, Shape69},
    {"B197", L"CMF Buds 2",                 Earbuds,    63, true,  true,  true,  true,  true,  true,  false, 0, 6,  0, 2, false, Shape69},
    {"B185", L"CMF Buds 2a",                Earbuds,    49, true,  false, false, false, false, true,  false, 0, 0,  0, 2, false, Shape69},
    {"B170", L"Nothing Headphone (1)",      Headphones, 63, false, true,  false, true,  false, true,  false, 4, 7,  2, 0, false, Shape35},
    {"B164", L"CMF Neckband Pro",           Neckband,   63, false, true,  false, false, false, true,  true,  0, 6,  0, 0, false, Shape69},
    {"B175", L"CMF Headphone Pro",          Headphones, 63, true,  false, false, false, false, true,  false, 4, 28, 2, 2, false, Shape35},
    {"B173", L"Nothing Ear (3)",            Earbuds,    63, false, true,  false, true,  true,  true,  false, 4, 6,  2, 0, false, Shape34},
    {"B189", L"CMF Clip Pro",               Earbuds,    -1, true,  true,  true,  false, false, true,  false, 4, 6,  0, 2, false, Shape34},
    {"B186", L"Nothing Headphone (a)",      Headphones, 63, false, true,  true,  false, false, true,  false, 4, 28, 0, 0, false, Shape35},
    {"B190", L"Nothing Ear (3a)",           Earbuds,    63, false, false, false, true,  false, true,  false, 4, 6,  0, 0, false, Shape34},
    {"B192", L"Nothing Headphone (1) Pro",  Headphones, 63, false, false, false, true,  false, true,  false, 4, 0,  2, 0, false, Shape35},
};

inline const Caps* capsFor(const std::string& base) {
    for (auto& c : AllCaps) if (base == c.base) return &c;
    return nullptr;
}
inline const FastPair* fastPair(const std::string& id) {
    for (auto& f : FastPairIds) if (_stricmp(id.c_str(), f.id) == 0) return &f;
    return nullptr;
}
// Falls back on the Bluetooth name when Fast Pair did not report a model.
inline const Caps* guessFromName(const std::wstring& name) {
    std::wstring n;
    for (wchar_t c : name) n += (wchar_t)towlower(c);
    struct { const wchar_t* key; const char* base; } table[] = {
        {L"buds 2 plus", "B184"}, {L"buds pro 2", "B172"}, {L"buds 2a", "B185"}, {L"buds 2", "B179"}, {L"buds pro", "B163"},
        {L"neckband", "B164"}, {L"headphone pro", "B175"}, {L"clip", "B189"}, {L"cmf buds", "B168"},
        {L"headphone (1) pro", "B192"}, {L"headphone (a)", "B186"}, {L"headphone (1)", "B170"},
        {L"ear (3a)", "B190"}, {L"ear (3)", "B173"}, {L"ear (open)", "B174"}, {L"ear (a)", "B162"}, {L"ear (2)", "B155"},
        {L"ear (stick)", "B157"}, {L"ear (1)", "B181"}, {L"nothing ear", "B171"},
    };
    for (auto& t : table) if (n.find(t.key) != std::wstring::npos) return capsFor(t.base);
    return nullptr;
}
inline bool looksLikeNothing(const std::wstring& name) {
    std::wstring n;
    for (wchar_t c : name) n += (wchar_t)towlower(c);
    return n.find(L"nothing") != std::wstring::npos || n.find(L"cmf") != std::wstring::npos || guessFromName(name) != nullptr;
}

// ---------------------------------------------------------------- feature tables

struct Option { const wchar_t* label; int wire; };

// EQ presets for listening-mode models, in ear (web)'s order (wire value = listening mode).
const Option ListeningPresets[] = {{L"Dirac OPTEO", 0}, {L"Pop", 3}, {L"Rock", 1}, {L"Electronic", 2}, {L"Enhance vocals", 4}, {L"Classical", 5}, {L"Custom", 6}};
const int CustomPreset = 6;

struct SpatialMode { const wchar_t* label; int bit, mode, head; };
const SpatialMode SpatialModes[] = {{L"Off", 0, 0, 0}, {L"Head tracked", 1, 1, 1}, {L"Fixed", 2, 1, 0}, {L"Concert", 8, 2, 0}, {L"Theatre", 16, 3, 0}, {L"Game", 32, 4, 0}};
inline int spatialIndex(int mode, int head) {
    if (head == 1) return 1;
    for (int i = 0; i < 6; ++i) if (SpatialModes[i].head == 0 && SpatialModes[i].mode == mode) return i;
    return 0;
}

const wchar_t* const CodecNames[] = {L"AAC", L"LHDC", L"LDAC"};

// Gesture slots of the espeon table: CMF Buds Pro 2 and CMF Buds 2 Plus (gesture device 2 = left, 3 = right).
struct GestureSlot { const wchar_t* label; int type; std::vector<Option> actions; };
inline const std::vector<GestureSlot>& espeonSlots() {
    static const std::vector<GestureSlot> slots = {
        {L"Double tap", 2, {{L"Play / pause", 2}, {L"Skip back", 8}, {L"Skip forward", 9}, {L"Voice assistant", 11}, {L"No action", 1}}},
        {L"Triple tap", 3, {{L"Skip back", 8}, {L"Skip forward", 9}, {L"Voice assistant", 11}, {L"No action", 1}}},
        {L"Tap and hold", 7, {{L"Noise control", 10}, {L"Voice assistant", 11}, {L"No action", 1}}},
        {L"Double tap and hold", 9, {{L"Volume up", 18}, {L"Volume down", 19}, {L"Voice assistant", 11}, {L"No action", 1}}},
    };
    return slots;
}
// "Noise control" on tap-and-hold cycles a subset of modes; the action code says which.
//   10: transparency, noise cancellation, off   20: noise cancellation, off
//   21: transparency, off                       22: transparency, noise cancellation
inline bool isNoiseControlAction(int a) { return a == 10 || a == 20 || a == 21 || a == 22; }
inline int noiseControlAction(bool transparency, bool nc, bool off) {
    if (transparency && nc && off) return 10;
    if (!transparency && nc && off) return 20;
    if (transparency && !nc && off) return 21;
    if (transparency && nc && !off) return 22;
    return 0;
}
inline void noiseControlParts(int a, bool& transparency, bool& nc, bool& off) {
    transparency = a != 20; nc = a != 21; off = a != 22;
}
} // namespace models
