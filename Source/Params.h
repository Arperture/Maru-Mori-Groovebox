#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/EngineParams.h"

// Params.h owns the parameter surface: APVTS layout (IDs + ranges + defaults)
// and the ParamRefs raw-pointer cache the processor snapshots from per block.
// EngineParams defaults in engine/EngineParams.h mirror the defaults here.

namespace maru::params {

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using P = juce::AudioProcessorValueTreeState;
    P::ParameterLayout layout;

    const auto pid = [](const char* id) { return juce::ParameterID{ id, 1 }; };
    const auto f = [&](const char* id, const char* name, float lo, float hi, float def) {
        return std::make_unique<juce::AudioParameterFloat>(
            pid(id), name, juce::NormalisableRange<float>(lo, hi), def);
    };
    const auto choice = [&](const char* id, const char* name,
                            std::initializer_list<const char*> opts, int def) {
        juce::StringArray sa;
        for (auto* o : opts) sa.add(o);
        return std::make_unique<juce::AudioParameterChoice>(pid(id), name, sa, def);
    };
    const auto onoff = [&](const char* id, const char* name, bool def) {
        return std::make_unique<juce::AudioParameterBool>(pid(id), name, def);
    };

    // ---- BASS ----
    layout.add(choice("bassWave",    "Bass Wave",     { "Saw", "Off", "Square" }, 0));
    layout.add(choice("bassSubWave", "Bass Sub Wave", { "Sine", "Triangle", "Square" }, 0));
    layout.add(choice("bassSubOct",  "Bass Sub Oct",  { "-1", "-2" }, 0));
    layout.add(f("bassSubLevel", "Bass Sub Level", 0.0f, 1.0f, 0.7f));
    layout.add(f("bassCutoff",   "Bass Cutoff",    0.0f, 1.0f, 0.35f));
    layout.add(f("bassRes",      "Bass Resonance", 0.0f, 1.0f, 0.25f));
    layout.add(f("bassEnvMod",   "Bass Env Mod",  -1.0f, 1.0f, 0.45f));
    layout.add(f("bassModAtt",   "Bass Mod Attack", 0.0f, 1.0f, 0.01f));
    layout.add(f("bassModDec",   "Bass Mod Decay",  0.0f, 1.0f, 0.35f));
    layout.add(f("bassVcaMod",   "Bass VCA Mod",  -1.0f, 1.0f, 0.0f));
    layout.add(f("bassAccent",   "Bass Accent",    0.0f, 1.0f, 0.6f));
    layout.add(f("bassDecay",    "Bass Decay",     0.0f, 1.0f, 0.4f));
    layout.add(f("bassAccDecay", "Bass Acc Decay", 0.0f, 1.0f, 0.2f));
    layout.add(f("bassGlide",    "Bass Glide",     0.001f, 1.0f, 0.055f));
    layout.add(f("bassLevel",    "Bass Trim",      0.0f, 1.0f, 0.8f));

    // ---- MIXER (bass, acid, drums, pad) ----
    static const char* kPart[4] = { "Bass", "Acid", "Drum", "Pad" };
    static const char* kMixIds[4][4] = {
        { "mixBassLvl", "mixBassPan", "mixBassDSend", "mixBassRSend" },
        { "mixAcidLvl", "mixAcidPan", "mixAcidDSend", "mixAcidRSend" },
        { "mixDrumLvl", "mixDrumPan", "mixDrumDSend", "mixDrumRSend" },
        { "mixPadLvl",  "mixPadPan",  "mixPadDSend",  "mixPadRSend"  },
    };
    for (int p = 0; p < 4; ++p) {
        const auto nm = [&](const char* what) {
            return juce::String(kPart[p]) + " " + what;
        };
        layout.add(f(kMixIds[p][0], nm("Level").toRawUTF8(),  0.0f, 1.0f, 0.8f));
        layout.add(f(kMixIds[p][1], nm("Pan").toRawUTF8(),   -1.0f, 1.0f, 0.0f));
        layout.add(f(kMixIds[p][2], nm("Delay Send").toRawUTF8(),  0.0f, 1.0f, 0.0f));
        layout.add(f(kMixIds[p][3], nm("Reverb Send").toRawUTF8(), 0.0f, 1.0f, 0.0f));
    }

    // ---- MASTER ----
    layout.add(f("masterGain", "Master Gain", 0.0f, 1.5f, 0.9f));
    layout.add(choice("masterHp", "Master HP", { "18 Hz", "70 Hz" }, 0));
    layout.add(f("masterBpm", "Internal BPM", 40.0f, 200.0f, 100.0f));

    return layout;
}

// Raw-value pointer cache: one getRawParameterValue lookup per param at
// construction, two atomic loads per param per block in snapshot().
class ParamRefs {
public:
    explicit ParamRefs(juce::AudioProcessorValueTreeState& apvts) {
        const auto r = [&](const char* id) {
            auto* p = apvts.getRawParameterValue(id);
            jassert(p != nullptr);
            return p;
        };
        bassWave = r("bassWave");       bassSubWave = r("bassSubWave");
        bassSubOct = r("bassSubOct");   bassSubLevel = r("bassSubLevel");
        bassCutoff = r("bassCutoff");   bassRes = r("bassRes");
        bassEnvMod = r("bassEnvMod");   bassModAtt = r("bassModAtt");
        bassModDec = r("bassModDec");   bassVcaMod = r("bassVcaMod");
        bassAccent = r("bassAccent");   bassDecay = r("bassDecay");
        bassAccDecay = r("bassAccDecay"); bassGlide = r("bassGlide");
        bassLevel = r("bassLevel");

        static const char* kMixIds[4][4] = {
            { "mixBassLvl", "mixBassPan", "mixBassDSend", "mixBassRSend" },
            { "mixAcidLvl", "mixAcidPan", "mixAcidDSend", "mixAcidRSend" },
            { "mixDrumLvl", "mixDrumPan", "mixDrumDSend", "mixDrumRSend" },
            { "mixPadLvl",  "mixPadPan",  "mixPadDSend",  "mixPadRSend"  },
        };
        for (int p = 0; p < 4; ++p) {
            mixLvl[p]   = r(kMixIds[p][0]);
            mixPan[p]   = r(kMixIds[p][1]);
            mixDSend[p] = r(kMixIds[p][2]);
            mixRSend[p] = r(kMixIds[p][3]);
        }

        masterGain = r("masterGain");
        masterHp   = r("masterHp");
        masterBpm  = r("masterBpm");
    }

    EngineParams snapshot() const {
        EngineParams e;
        e.bass.wave      = (int) bassWave->load();
        e.bass.subWave   = (int) bassSubWave->load();
        e.bass.subOct    = 1 + (int) bassSubOct->load();
        e.bass.subLevel  = bassSubLevel->load();
        e.bass.cutoff    = bassCutoff->load();
        e.bass.res       = bassRes->load();
        e.bass.envMod    = bassEnvMod->load();
        e.bass.modAtt    = bassModAtt->load();
        e.bass.modDec    = bassModDec->load();
        e.bass.vcaModDepth = bassVcaMod->load();
        e.bass.accent    = bassAccent->load();
        e.bass.decayNorm = bassDecay->load();
        e.bass.decayAcc  = bassAccDecay->load();
        e.bass.glide     = bassGlide->load();
        e.bass.level     = bassLevel->load();

        for (int p = 0; p < 4; ++p) {
            e.mix.level[p]   = mixLvl[p]->load();
            e.mix.pan[p]     = mixPan[p]->load();
            e.mix.dlySend[p] = mixDSend[p]->load();
            e.mix.vrbSend[p] = mixRSend[p]->load();
        }

        e.master.gain   = masterGain->load();
        e.master.hpMode = (int) masterHp->load();
        e.freeBpm       = (double) masterBpm->load();
        return e;
    }

private:
    std::atomic<float>* bassWave{};   std::atomic<float>* bassSubWave{};
    std::atomic<float>* bassSubOct{}; std::atomic<float>* bassSubLevel{};
    std::atomic<float>* bassCutoff{}; std::atomic<float>* bassRes{};
    std::atomic<float>* bassEnvMod{}; std::atomic<float>* bassModAtt{};
    std::atomic<float>* bassModDec{}; std::atomic<float>* bassVcaMod{};
    std::atomic<float>* bassAccent{}; std::atomic<float>* bassDecay{};
    std::atomic<float>* bassAccDecay{}; std::atomic<float>* bassGlide{};
    std::atomic<float>* bassLevel{};
    std::atomic<float>* mixLvl[4]{};  std::atomic<float>* mixPan[4]{};
    std::atomic<float>* mixDSend[4]{}; std::atomic<float>* mixRSend[4]{};
    std::atomic<float>* masterGain{}; std::atomic<float>* masterHp{};
    std::atomic<float>* masterBpm{};
};

} // namespace maru::params
