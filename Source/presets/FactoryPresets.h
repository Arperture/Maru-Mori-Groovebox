#pragma once
#include <iterator>
#include "../engine/Patterns.h"

// Factory presets: param overrides applied over defaults + a groove builder.
// Voicing intent: Carbon Based Lifeforms — deep clean sub, slow acid, soft
// dubby drums, huge tails. TUNE BY EAR: these are starting points for Drew.

namespace maru::presets {

struct KV { const char* id; float v; }; // v in the parameter's natural units

using GrooveBuilder = void (*)(GrooveBanks&);

struct FactoryPreset {
    const char* name;
    const KV* kv;
    int n;
    GrooveBuilder buildGroove; // nullptr = leave patterns untouched
};

// ---- shared pattern builders ------------------------------------------------

namespace build {

inline void bassStep(BassPattern& p, int ix, int note, bool acc = false,
                     bool slide = false, int gateLen = 2) {
    auto& s = p.steps[ix];
    s.gate = true; s.note = note; s.accent = acc; s.slide = slide;
    s.gateLen = gateLen; s.slideT = 1; s.cvOct = 0.0f;
}

inline void clearBass(BassPattern& p) { for (auto& s : p.steps) s.gate = false; }
inline void clearAcid(AcidPattern& p) { for (auto& s : p.steps) s.gate = false; }

inline void acidStep(AcidPattern& p, int ix, int note, bool acc = false,
                     bool slide = false) {
    auto& s = p.steps[ix];
    s.gate = true; s.note = note; s.accent = acc; s.slide = slide;
}

inline void drumHit(DrumGrid& g, int lane, int step, bool acc = false) {
    g.cells[lane][step] = { true, acc };
}

inline void padChords(PadPattern& p, const int (&c0)[4], const int (&c1)[4],
                      const int (&c2)[4], const int (&c3)[4]) {
    const int* cs[4] = { c0, c1, c2, c3 };
    for (int c = 0; c < 4; ++c) {
        p.chords[c].count = 4;
        for (int n = 0; n < 4; ++n) p.chords[c].notes[n] = cs[c][n];
    }
}

inline void padBarPerChord(PadPattern& p) { // chord change every 2 steps, ties between
    for (int s = 0; s < 16; ++s)
        p.steps[s] = (s & 1) == 0 ? PadStep{ true, (s / 2) % 4, false }
                                  : PadStep{ false, 0, true };
}

// The canonical Maru Mori groove (also the RenderTest demo): A-minor psybient.
inline void hydroponic(GrooveBanks& g) {
    for (int b = 0; b < kNumBanks; ++b) {
        auto& bass = g.bass[b];
        clearBass(bass);
        bassStep(bass, 0, 33, true);
        bassStep(bass, 3, 33, false, false, 1);
        bassStep(bass, 6, 40);
        bassStep(bass, 7, 38, false, true);
        bassStep(bass, 10, 33);
        bassStep(bass, 12, 45, true, false, 1);
        bassStep(bass, 14, 31, false, false, 3);

        auto& acid = g.acid[b];
        clearAcid(acid);
        acidStep(acid, 0, 45, true);
        acidStep(acid, 2, 45);
        acidStep(acid, 5, 48);
        acidStep(acid, 6, 50, false, true);
        acidStep(acid, 9, 45);
        acidStep(acid, 12, 57, true);
        acidStep(acid, 13, 55, false, true);

        auto& d = g.drums[b];
        d = {};
        drumHit(d, 0, 0, true); drumHit(d, 0, 8); drumHit(d, 0, 10);
        drumHit(d, 1, 4); drumHit(d, 1, 12);
        for (int s = 2; s < 16; s += 4) drumHit(d, 3, s);
        drumHit(d, 4, 14);
        drumHit(d, 7, 6); drumHit(d, 7, 15);

        auto& pad = g.pad[b];
        padChords(pad, { 57, 60, 64, 67 }, { 53, 57, 60, 64 },
                       { 48, 55, 60, 64 }, { 52, 55, 59, 62 });
        padBarPerChord(pad);

        // bank B: busier drums; bank C: half bass; bank D: breakdown
        if (b == 1) { for (int s = 1; s < 16; s += 2) drumHit(d, 3, s); }
        if (b == 2) { clearBass(g.bass[b]); bassStep(g.bass[b], 0, 33, false, false, 3);
                      bassStep(g.bass[b], 8, 31, false, false, 3); }
        if (b == 3) { g.drums[b] = {}; drumHit(g.drums[b], 4, 0); }
    }
}

inline void silentRunning(GrooveBanks& g) {
    hydroponic(g);
    for (int b = 0; b < kNumBanks; ++b) {
        auto& bass = g.bass[b];
        clearBass(bass);
        bassStep(bass, 0, 33, false, false, 3);
        bassStep(bass, 6, 33, false, false, 1);
        bassStep(bass, 8, 36, false, true, 3);
        bassStep(bass, 14, 31, false, false, 2);
        clearAcid(g.acid[b]);
        auto& d = g.drums[b];
        d = {};
        drumHit(d, 0, 0, true); drumHit(d, 0, 10);
        drumHit(d, 6, 4); drumHit(d, 6, 12, true);
        drumHit(d, 7, 2); drumHit(d, 7, 7); drumHit(d, 7, 13);
    }
}

inline void mos6581(GrooveBanks& g) {
    hydroponic(g);
    for (int b = 0; b < kNumBanks; ++b) {
        auto& acid = g.acid[b];
        clearAcid(acid);
        acidStep(acid, 0, 33, true);
        acidStep(acid, 1, 33);
        acidStep(acid, 3, 45, false, true);
        acidStep(acid, 4, 33);
        acidStep(acid, 6, 36, true);
        acidStep(acid, 7, 35, false, true);
        acidStep(acid, 10, 33);
        acidStep(acid, 11, 48, true, true);
        acidStep(acid, 14, 31);
        auto& d = g.drums[b];
        for (int s = 0; s < 16; s += 2) drumHit(d, 3, s);
    }
}

inline void abiogenesisG(GrooveBanks& g) {
    hydroponic(g);
    for (int b = 0; b < kNumBanks; ++b) {
        g.drums[b] = {};
        clearAcid(g.acid[b]);
        auto& bass = g.bass[b];
        clearBass(bass);
        bassStep(bass, 0, 33, false, false, 3); // drone anchor (VCA mod opens it)
        padChords(g.pad[b], { 57, 62, 64, 69 }, { 55, 60, 62, 67 },
                            { 53, 58, 60, 65 }, { 52, 57, 59, 64 });
    }
}

} // namespace build

// ---- the bank --------------------------------------------------------------

inline constexpr KV kHydroponicKV[] = {
    { "masterBpm", 100.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 1.0f },
    { "seqBassSwing", 0.15f }, { "seqAcidOn", 1.0f }, { "seqAcidRate", 0.0f },
    { "seqAcidSwing", 0.15f }, { "seqDrumOn", 1.0f }, { "seqDrumRate", 0.0f },
    { "seqDrumSwing", 0.15f }, { "seqPadOn", 1.0f }, { "seqPadRate", 6.0f },
    { "bassCutoff", 0.32f }, { "bassRes", 0.45f }, { "bassEnvMod", 0.5f },
    { "bassSubLevel", 0.8f },
    { "acidCutoff", 0.25f }, { "acidRes", 0.65f }, { "acidEnvMod", 0.4f },
    { "padAttack", 0.6f },
    { "mixAcidLvl", 0.55f }, { "mixDrumLvl", 0.7f }, { "mixPadLvl", 0.5f },
    { "mixBassDSend", 0.5f }, { "mixBassRSend", 0.35f },
    { "mixAcidDSend", 0.7f }, { "mixAcidRSend", 0.3f },
    { "mixDrumDSend", 0.15f }, { "mixDrumRSend", 0.25f }, { "mixPadRSend", 0.6f },
    { "dlyDiv", 3.0f }, { "dlyFb", 0.55f }, { "dlyMode", 1.0f },
    { "dlyToVerb", 0.3f }, { "vrbDecay", 0.65f }, { "vrbShim", 0.25f },
};

inline constexpr KV kSilentRunningKV[] = {
    { "masterBpm", 90.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 1.0f },
    { "seqDrumOn", 1.0f }, { "seqDrumRate", 1.0f }, { "seqDrumSwing", 0.25f },
    { "seqPadOn", 1.0f }, { "seqPadRate", 6.0f },
    { "bassWave", 1.0f },                       // main osc OFF: pure sub
    { "bassSubWave", 0.0f }, { "bassSubLevel", 1.0f }, { "bassCutoff", 0.3f },
    { "bassVcaDecay", 0.9f }, { "bassFr", 1.0f },
    { "mixBassDSend", 0.25f }, { "mixDrumDSend", 0.6f }, { "mixDrumRSend", 0.4f },
    { "mixPadLvl", 0.45f }, { "mixPadRSend", 0.7f },
    { "dlyMode", 2.0f },                        // tape
    { "dlyDiv", 5.0f }, { "dlyFb", 0.65f }, { "dlyTone", 0.3f },
    { "dlyToVerb", 0.4f }, { "vrbDecay", 0.75f }, { "vrbSize", 0.9f },
    { "padCutoff", 0.45f }, { "padAttack", 0.7f }, { "padRelease", 0.75f },
};

inline constexpr KV kMos6581KV[] = {
    { "masterBpm", 108.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 1.0f },
    { "seqAcidOn", 1.0f }, { "seqAcidRate", 0.0f }, { "seqAcidDir", 2.0f },
    { "seqDrumOn", 1.0f }, { "seqDrumRate", 0.0f },
    { "acidWave", 1.0f },                       // square acid
    { "acidCutoff", 0.35f }, { "acidRes", 0.8f }, { "acidEnvMod", 0.6f },
    { "acidAccent", 0.85f }, { "acidDecay", 0.3f },
    { "mixAcidLvl", 0.7f }, { "mixAcidDSend", 0.6f },
    { "mixPadLvl", 0.0f },
    { "dlyDiv", 1.0f }, { "dlyFb", 0.45f },     // 1/8T dub
    { "vrbDecay", 0.5f },
    { "masterAccent", 0.7f },
};

inline constexpr KV kAbiogenesisKV[] = {
    { "masterBpm", 80.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 6.0f },
    { "seqPadOn", 1.0f }, { "seqPadRate", 6.0f },
    { "bassVcaMod", -0.6f },                    // drone mode: mod env opens VCA
    { "bassModAtt", 0.7f }, { "bassModDec", 0.9f }, { "bassSubLevel", 0.9f },
    { "bassCutoff", 0.25f },
    { "mixBassRSend", 0.5f }, { "mixPadLvl", 0.6f }, { "mixPadRSend", 0.8f },
    { "padAttack", 0.8f }, { "padRelease", 0.85f }, { "padLfoDepth", 0.3f },
    { "vrbDecay", 0.85f }, { "vrbSize", 1.0f }, { "vrbShim", 0.45f },
    { "vrbShimInt", 4.0f },                     // +12&+7 dual shimmer
    { "dlyRet", 0.0f },
};

inline constexpr KV kPhotosynthesisKV[] = {
    { "masterBpm", 104.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 1.0f },
    { "seqAcidOn", 1.0f }, { "seqAcidRate", 1.0f }, { "seqAcidSwing", 0.2f },
    { "seqDrumOn", 1.0f }, { "seqDrumRate", 0.0f }, { "seqDrumSwing", 0.2f },
    { "seqPadOn", 1.0f }, { "seqPadRate", 6.0f },
    { "acidOctave", 2.0f },                     // +1: lead register
    { "acidCutoff", 0.45f }, { "acidRes", 0.4f }, { "acidDecay", 0.6f },
    { "bassCutoff", 0.4f }, { "bassSubLevel", 0.6f },
    { "padCutoff", 0.7f }, { "padLfoDepth", 0.25f },
    { "mixAcidLvl", 0.45f }, { "mixAcidDSend", 0.8f }, { "mixAcidRSend", 0.4f },
    { "mixPadLvl", 0.55f }, { "mixPadRSend", 0.5f }, { "mixDrumRSend", 0.3f },
    { "dlyDiv", 5.0f }, { "dlyFb", 0.5f },      // 1/4. throws
    { "vrbDecay", 0.6f }, { "vrbShim", 0.35f }, { "vrbShimInt", 1.0f }, // +7
};

inline constexpr KV kInterloperKV[] = {
    { "masterBpm", 96.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 1.0f },
    { "seqAcidOn", 1.0f }, { "seqAcidRate", 0.0f }, { "seqAcidDir", 3.0f }, // RANDOM
    { "seqDrumOn", 1.0f }, { "seqDrumRate", 1.0f }, { "seqDrumDir", 2.0f }, // PEND
    { "seqPadOn", 1.0f }, { "seqPadRate", 6.0f },
    { "acidCutoff", 0.2f }, { "acidRes", 0.7f }, { "acidAccent", 0.9f },
    { "mixAcidDSend", 0.75f }, { "mixDrumDSend", 0.35f },
    { "dlyDiv", 1.0f }, { "dlyFb", 0.6f }, { "dlyMode", 0.0f },
    { "vrbDecay", 0.7f }, { "vrbLoDamp", 0.3f },
    { "masterAccent", 0.65f },
};

inline constexpr KV kSleepersKV[] = {
    { "masterBpm", 72.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 6.0f },
    { "seqPadOn", 1.0f }, { "seqPadRate", 6.0f }, { "seqDrumOn", 1.0f },
    { "seqDrumRate", 1.0f },
    { "bassWave", 1.0f }, { "bassSubLevel", 1.0f }, { "bassSubWave", 0.0f },
    { "bassVcaMod", -0.4f }, { "bassModDec", 0.85f }, { "bassVcaDecay", 1.0f },
    { "mixBassRSend", 0.3f }, { "mixDrumLvl", 0.5f }, { "mixDrumRSend", 0.6f },
    { "mixPadLvl", 0.6f }, { "mixPadRSend", 0.75f }, { "mixPadDSend", 0.2f },
    { "padAttack", 0.85f }, { "padRelease", 0.9f }, { "padCutoff", 0.4f },
    { "dlyMode", 2.0f }, { "dlyDiv", 7.0f }, { "dlyFb", 0.7f }, { "dlyTone", 0.25f },
    { "vrbDecay", 0.9f }, { "vrbSize", 1.0f }, { "vrbHiDamp", 0.6f },
};

inline constexpr KV kTensorKV[] = {
    { "masterBpm", 118.0f }, { "seqBassOn", 1.0f }, { "seqBassRate", 0.0f },
    { "seqAcidOn", 1.0f }, { "seqAcidRate", 0.0f }, { "seqAcidSwing", 0.1f },
    { "seqDrumOn", 1.0f }, { "seqDrumRate", 0.0f },
    { "bassCutoff", 0.45f }, { "bassRes", 0.55f }, { "bassEnvMod", 0.65f },
    { "bassDecay", 0.3f }, { "bassAccent", 0.8f },
    { "acidCutoff", 0.3f }, { "acidRes", 0.75f }, { "acidEnvMod", 0.55f },
    { "mixBassDSend", 0.3f }, { "mixAcidDSend", 0.55f }, { "mixDrumDSend", 0.2f },
    { "dlyDiv", 0.0f }, { "dlyFb", 0.5f },      // 1/16 stutter
    { "vrbDecay", 0.45f }, { "masterAccent", 0.75f },
};

inline const FactoryPreset kFactory[] = {
    { "Hydroponic Garden", kHydroponicKV, (int) std::size(kHydroponicKV), build::hydroponic },
    { "Silent Running",    kSilentRunningKV, (int) std::size(kSilentRunningKV), build::silentRunning },
    { "MOS 6581",          kMos6581KV, (int) std::size(kMos6581KV), build::mos6581 },
    { "Abiogenesis",       kAbiogenesisKV, (int) std::size(kAbiogenesisKV), build::abiogenesisG },
    { "Photosynthesis",    kPhotosynthesisKV, (int) std::size(kPhotosynthesisKV), build::hydroponic },
    { "Interloper",        kInterloperKV, (int) std::size(kInterloperKV), build::mos6581 },
    { "World of Sleepers", kSleepersKV, (int) std::size(kSleepersKV), build::silentRunning },
    { "Tensor",            kTensorKV, (int) std::size(kTensorKV), build::hydroponic },
};
inline constexpr int kNumFactory = (int) std::size(kFactory);

} // namespace maru::presets
