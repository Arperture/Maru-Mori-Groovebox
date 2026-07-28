#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/EngineParams.h"

// Params.h owns the parameter surface: APVTS layout (IDs + ranges + defaults)
// and the ParamRefs raw-pointer cache the processor snapshots from per block.
// EngineParams defaults in engine/EngineParams.h mirror the defaults here.

namespace maru::params {

namespace detail {
inline const char* kMixIds[4][4] = {
    { "mixBassLvl", "mixBassPan", "mixBassDSend", "mixBassRSend" },
    { "mixAcidLvl", "mixAcidPan", "mixAcidDSend", "mixAcidRSend" },
    { "mixDrumLvl", "mixDrumPan", "mixDrumDSend", "mixDrumRSend" },
    { "mixPadLvl",  "mixPadPan",  "mixPadDSend",  "mixPadRSend"  },
};
inline const char* kSeqIds[4][6] = {
    { "seqBassOn", "seqBassRate", "seqBassLen", "seqBassDir", "seqBassSwing", "seqBassBank" },
    { "seqAcidOn", "seqAcidRate", "seqAcidLen", "seqAcidDir", "seqAcidSwing", "seqAcidBank" },
    { "seqDrumOn", "seqDrumRate", "seqDrumLen", "seqDrumDir", "seqDrumSwing", "seqDrumBank" },
    { "seqPadOn",  "seqPadRate",  "seqPadLen",  "seqPadDir",  "seqPadSwing",  "seqPadBank"  },
};
inline const char* kPartName[4] = { "Bass", "Acid", "Drum", "Pad" };
inline const char* kPadName[8] = { "Kick", "Snare", "Clap", "CH", "OH", "Tom", "Rim", "Shaker" };
} // namespace detail

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using P = juce::AudioProcessorValueTreeState;
    P::ParameterLayout layout;

    const auto pid = [](const char* id) { return juce::ParameterID{ id, 1 }; };
    const auto f = [&](const char* id, juce::String name, float lo, float hi, float def) {
        return std::make_unique<juce::AudioParameterFloat>(
            pid(id), name, juce::NormalisableRange<float>(lo, hi), def);
    };
    const auto choice = [&](const char* id, juce::String name,
                            std::initializer_list<const char*> opts, int def) {
        juce::StringArray sa;
        for (auto* o : opts) sa.add(o);
        return std::make_unique<juce::AudioParameterChoice>(pid(id), name, sa, def);
    };
    const auto onoff = [&](const char* id, juce::String name, bool def) {
        return std::make_unique<juce::AudioParameterBool>(pid(id), name, def);
    };
    const auto intp = [&](const char* id, juce::String name, int lo, int hi, int def) {
        return std::make_unique<juce::AudioParameterInt>(pid(id), name, lo, hi, def);
    };
    const auto midiCh = [&](const char* id, juce::String name, int defIndex) {
        juce::StringArray sa;
        for (int c = 1; c <= 16; ++c) sa.add(juce::String(c));
        return std::make_unique<juce::AudioParameterChoice>(pid(id), name, sa, defIndex);
    };

    // ---- BASS (avalon voice) ----
    layout.add(choice("bassWave",    "Bass Wave",     { "Saw", "Off", "Square" }, 0));
    layout.add(choice("bassSubWave", "Bass Sub Wave", { "Saw", "Triangle", "Square" }, 0));
    layout.add(choice("bassSubOct",  "Bass Sub Oct",  { "-1", "-2" }, 0));
    layout.add(f("bassSubLevel", "Bass Sub Level",  0.0f, 1.0f, 0.7f));
    layout.add(f("bassCutoff",   "Bass Cutoff",     0.0f, 1.0f, 0.4f));
    layout.add(f("bassRes",      "Bass Resonance",  0.0f, 1.0f, 0.3f));
    layout.add(f("bassEnvMod",   "Bass Env Mod",    0.0f, 1.0f, 0.45f));
    layout.add(f("bassTracking", "Bass Tracking",   0.0f, 1.0f, 0.3f));
    layout.add(f("bassDecay",    "Bass Env Decay",  0.0f, 1.0f, 0.5f));
    layout.add(f("bassAccDecay", "Bass Acc Decay",  0.0f, 1.0f, 0.35f));
    layout.add(f("bassAccent",   "Bass Accent",     0.0f, 1.0f, 0.6f));
    layout.add(f("bassVcaDecay", "Bass VCA Decay",  0.0f, 1.0f, 0.75f));
    layout.add(f("bassModAtt",   "Bass Mod Attack", 0.0f, 1.0f, 0.25f));
    layout.add(f("bassModDec",   "Bass Mod Decay",  0.0f, 1.0f, 0.4f));
    layout.add(f("bassVcfMod",   "Bass VCF Mod",   -1.0f, 1.0f, 0.0f));
    layout.add(f("bassVcaMod",   "Bass VCA Mod",   -1.0f, 1.0f, 0.0f));
    layout.add(f("bassGlide",    "Bass Glide",      0.001f, 1.0f, 0.1f));
    layout.add(choice("bassFr",  "Bass Range",      { "70 Hz", "Full" }, 1));
    layout.add(f("bassLevel",    "Bass Trim",       0.0f, 1.0f, 0.8f));

    // ---- ACID (tb303 voice) ----
    layout.add(choice("acidWave",  "Acid Wave", { "Saw", "Square" }, 0));
    layout.add(f("acidCutoff", "Acid Cutoff",    0.0f, 1.0f, 0.35f));
    layout.add(f("acidRes",    "Acid Resonance", 0.0f, 1.0f, 0.5f));
    layout.add(f("acidEnvMod", "Acid Env Mod",   0.0f, 1.0f, 0.5f));
    layout.add(f("acidDecay",  "Acid Decay",     0.0f, 1.0f, 0.4f));
    layout.add(f("acidAccent", "Acid Accent",    0.0f, 1.0f, 0.6f));
    layout.add(choice("acidOctave", "Acid Octave", { "-1", "0", "+1" }, 1));
    layout.add(f("acidLevel",  "Acid Trim",      0.0f, 1.0f, 0.8f));

    // ---- DRUMS: 8 pads x {tune, decay, cutoff, level, pan, choke} ----
    // defaults: CH+OH share choke group 1; light default panning
    {
        static const float kPadPan[8]   = { 0.0f, 0.0f, 0.15f, -0.2f, -0.2f, 0.3f, -0.35f, 0.4f };
        static const int   kPadChoke[8] = { 0, 0, 0, 1, 1, 0, 0, 0 };
        for (int d = 0; d < 8; ++d) {
            const juce::String base = "drum" + juce::String(d + 1);
            const juce::String nm = detail::kPadName[d];
            layout.add(f((base + "Tune").toRawUTF8(),  nm + " Tune", -24.0f, 24.0f, 0.0f));
            layout.add(f((base + "Decay").toRawUTF8(), nm + " Decay", 0.0f, 1.0f, 1.0f));
            layout.add(f((base + "Cut").toRawUTF8(),   nm + " Cutoff", 0.0f, 1.0f, 1.0f));
            layout.add(f((base + "Lvl").toRawUTF8(),   nm + " Level", 0.0f, 1.0f, 0.8f));
            layout.add(f((base + "Pan").toRawUTF8(),   nm + " Pan", -1.0f, 1.0f, kPadPan[d]));
            layout.add(choice((base + "Choke").toRawUTF8(), nm + " Choke",
                { "Off", "1", "2", "3", "4" }, kPadChoke[d]));
        }
    }

    // ---- PAD (juno-flavored poly) ----
    layout.add(f("padPwm",      "Pad PWM",       0.0f, 1.0f, 0.4f));
    layout.add(f("padSubLevel", "Pad Sub Level", 0.0f, 1.0f, 0.4f));
    layout.add(f("padCutoff",   "Pad Cutoff",    0.0f, 1.0f, 0.55f));
    layout.add(f("padRes",      "Pad Resonance", 0.0f, 1.0f, 0.15f));
    layout.add(f("padAttack",   "Pad Attack",    0.0f, 1.0f, 0.55f));
    layout.add(f("padDecayEnv", "Pad Decay",     0.0f, 1.0f, 0.5f));
    layout.add(f("padSustain",  "Pad Sustain",   0.0f, 1.0f, 0.7f));
    layout.add(f("padRelease",  "Pad Release",   0.0f, 1.0f, 0.6f));
    layout.add(f("padLfoRate",  "Pad LFO Rate",  0.0f, 1.0f, 0.3f));
    layout.add(f("padLfoDepth", "Pad LFO Depth", 0.0f, 1.0f, 0.15f));
    layout.add(f("padLfoDelay", "Pad LFO Delay", 0.0f, 1.0f, 0.5f));
    layout.add(choice("padChorus", "Pad Chorus", { "Off", "I", "II", "Ensemble" }, 3));
    layout.add(f("padLevel",    "Pad Trim",      0.0f, 1.0f, 0.7f));

    // ---- SEQUENCERS (bass live at M1; acid/drums/pad params pre-wired) ----
    for (int p = 0; p < 4; ++p) {
        const juce::String nm = detail::kPartName[p];
        layout.add(onoff(detail::kSeqIds[p][0], nm + " Seq On", false));
        layout.add(choice(detail::kSeqIds[p][1], nm + " Seq Rate",
            { "1/16", "1/8", "1/16T", "1/8T", "1/4", "1/2", "1/1" }, 0));
        layout.add(intp(detail::kSeqIds[p][2], nm + " Seq Length", 1, 16, 16));
        layout.add(choice(detail::kSeqIds[p][3], nm + " Seq Dir",
            { "Forward", "Reverse", "Pendulum", "Random" }, 0));
        layout.add(f(detail::kSeqIds[p][4], nm + " Seq Swing", 0.0f, 1.0f, 0.0f));
        layout.add(choice(detail::kSeqIds[p][5], nm + " Seq Bank",
            { "A", "B", "C", "D" }, 0)); // applied at the next bar boundary
    }

    // ---- MIXER ----
    for (int p = 0; p < 4; ++p) {
        const juce::String nm = detail::kPartName[p];
        layout.add(f(detail::kMixIds[p][0], nm + " Level",  0.0f, 1.0f, 0.8f));
        layout.add(f(detail::kMixIds[p][1], nm + " Pan",   -1.0f, 1.0f, 0.0f));
        layout.add(f(detail::kMixIds[p][2], nm + " Delay Send",  0.0f, 1.0f, 0.0f));
        layout.add(f(detail::kMixIds[p][3], nm + " Reverb Send", 0.0f, 1.0f, 0.0f));
    }

    // ---- DELAY (send bus) ----
    layout.add(onoff("dlySync",  "Delay Sync", true));
    layout.add(choice("dlyDiv",  "Delay Division",
        { "1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2", "1/2.", "1/1" }, 4));
    layout.add(f("dlyTime",   "Delay Time",     0.03f, 4.0f, 0.5f));
    layout.add(f("dlyFb",     "Delay Feedback", 0.0f, 0.85f, 0.45f));
    layout.add(f("dlyTone",   "Delay Tone",     0.0f, 1.0f, 0.5f));
    layout.add(choice("dlyMode", "Delay Mode",  { "Stereo", "Ping-Pong", "Tape" }, 1));
    layout.add(f("dlyToVerb", "Delay To Verb",  0.0f, 0.6f, 0.0f));
    layout.add(f("dlyRet",    "Delay Return",   0.0f, 1.0f, 0.8f));

    // ---- REVERB (Bloom, send bus) ----
    layout.add(f("vrbDecay",  "Verb Decay",    0.0f, 1.0f, 0.55f));
    layout.add(f("vrbSize",   "Verb Size",     0.0f, 1.0f, 0.75f));
    layout.add(f("vrbPre",    "Verb Predelay", 0.0f, 250.0f, 20.0f));
    layout.add(f("vrbMod",    "Verb Mod",      0.0f, 1.0f, 0.4f));
    layout.add(f("vrbLoDamp", "Verb Low Damp", 0.0f, 1.0f, 0.15f));
    layout.add(f("vrbHiDamp", "Verb High Damp", 0.0f, 1.0f, 0.35f));
    layout.add(f("vrbShim",   "Verb Shimmer",  0.0f, 1.0f, 0.0f));
    layout.add(choice("vrbShimInt", "Verb Shim Interval",
        { "+12", "+7", "+5", "-12", "+12&+7" }, 0));
    layout.add(onoff("vrbFreeze", "Verb Freeze", false));
    layout.add(f("vrbRet",    "Verb Return",   0.0f, 1.0f, 0.8f));

    // ---- MIDI ROUTING ----
    // per-part receive channel + the controller-focus selector (the CTRL
    // buttons on each module; 0 = off = channel routing)
    layout.add(midiCh("bassMidiCh", "Bass MIDI Ch", 0));
    layout.add(midiCh("acidMidiCh", "Acid MIDI Ch", 1));
    layout.add(midiCh("drumMidiCh", "Drum MIDI Ch", 2));
    layout.add(midiCh("padMidiCh",  "Pad MIDI Ch",  3));
    layout.add(choice("midiFocus",  "MIDI Focus",
        { "Off", "Bass", "Acid", "Drums", "Pad" }, 0));

    // ---- MASTER ----
    layout.add(f("masterGain", "Master Gain", 0.0f, 1.5f, 0.9f));
    layout.add(choice("masterHp", "Master HP", { "18 Hz", "70 Hz" }, 0));
    layout.add(f("masterBpm", "Internal BPM", 40.0f, 200.0f, 100.0f));
    layout.add(f("masterAccent", "Global Accent", 0.0f, 1.0f, 0.5f));

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
        bassEnvMod = r("bassEnvMod");   bassTracking = r("bassTracking");
        bassDecay = r("bassDecay");     bassAccDecay = r("bassAccDecay");
        bassAccent = r("bassAccent");   bassVcaDecay = r("bassVcaDecay");
        bassModAtt = r("bassModAtt");   bassModDec = r("bassModDec");
        bassVcfMod = r("bassVcfMod");   bassVcaMod = r("bassVcaMod");
        bassGlide = r("bassGlide");     bassFr = r("bassFr");
        bassLevel = r("bassLevel");

        acidWave = r("acidWave");     acidCutoff = r("acidCutoff");
        acidRes = r("acidRes");       acidEnvMod = r("acidEnvMod");
        acidDecay = r("acidDecay");   acidAccent = r("acidAccent");
        acidOctave = r("acidOctave"); acidLevel = r("acidLevel");

        padPwm = r("padPwm");         padSubLevel = r("padSubLevel");
        padCutoff = r("padCutoff");   padRes = r("padRes");
        padAttack = r("padAttack");   padDecayEnv = r("padDecayEnv");
        padSustain = r("padSustain"); padRelease = r("padRelease");
        padLfoRate = r("padLfoRate"); padLfoDepth = r("padLfoDepth");
        padLfoDelay = r("padLfoDelay"); padChorus = r("padChorus");
        padLevel = r("padLevel");

        for (int d = 0; d < 8; ++d) {
            const juce::String base = "drum" + juce::String(d + 1);
            drumTune[d]  = r((base + "Tune").toRawUTF8());
            drumDecay[d] = r((base + "Decay").toRawUTF8());
            drumCut[d]   = r((base + "Cut").toRawUTF8());
            drumLvl[d]   = r((base + "Lvl").toRawUTF8());
            drumPan[d]   = r((base + "Pan").toRawUTF8());
            drumChoke[d] = r((base + "Choke").toRawUTF8());
        }

        for (int p = 0; p < 4; ++p) {
            seqOn[p]    = r(detail::kSeqIds[p][0]);
            seqRate[p]  = r(detail::kSeqIds[p][1]);
            seqLen[p]   = r(detail::kSeqIds[p][2]);
            seqDir[p]   = r(detail::kSeqIds[p][3]);
            seqSwing[p] = r(detail::kSeqIds[p][4]);
            seqBank[p]  = r(detail::kSeqIds[p][5]);
            mixLvl[p]   = r(detail::kMixIds[p][0]);
            mixPan[p]   = r(detail::kMixIds[p][1]);
            mixDSend[p] = r(detail::kMixIds[p][2]);
            mixRSend[p] = r(detail::kMixIds[p][3]);
        }

        dlySync = r("dlySync");   dlyDiv = r("dlyDiv");
        dlyTime = r("dlyTime");   dlyFb = r("dlyFb");
        dlyTone = r("dlyTone");   dlyMode = r("dlyMode");
        dlyToVerb = r("dlyToVerb"); dlyRet = r("dlyRet");

        vrbDecay = r("vrbDecay"); vrbSize = r("vrbSize");
        vrbPre = r("vrbPre");     vrbMod = r("vrbMod");
        vrbLoDamp = r("vrbLoDamp"); vrbHiDamp = r("vrbHiDamp");
        vrbShim = r("vrbShim");   vrbShimInt = r("vrbShimInt");
        vrbFreeze = r("vrbFreeze"); vrbRet = r("vrbRet");

        midiChRef[0] = r("bassMidiCh"); midiChRef[1] = r("acidMidiCh");
        midiChRef[2] = r("drumMidiCh"); midiChRef[3] = r("padMidiCh");
        midiFocus = r("midiFocus");

        masterGain = r("masterGain");
        masterHp   = r("masterHp");
        masterBpm  = r("masterBpm");
        masterAccent = r("masterAccent");
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
        e.bass.tracking  = bassTracking->load();
        e.bass.envDecay  = bassDecay->load();
        e.bass.accDecay  = bassAccDecay->load();
        e.bass.accent    = bassAccent->load();
        e.bass.vcaDecay  = bassVcaDecay->load();
        e.bass.modAtt    = bassModAtt->load();
        e.bass.modDec    = bassModDec->load();
        e.bass.vcfModDepth = bassVcfMod->load();
        e.bass.vcaModDepth = bassVcaMod->load();
        e.bass.glide     = bassGlide->load();
        e.bass.fr        = (int) bassFr->load();
        e.bass.level     = bassLevel->load();

        e.acid.wave   = (int) acidWave->load();
        e.acid.cutoff = acidCutoff->load();
        e.acid.res    = acidRes->load();
        e.acid.envMod = acidEnvMod->load();
        e.acid.decay  = acidDecay->load();
        e.acid.accent = acidAccent->load();
        e.acid.octave = (int) acidOctave->load() - 1;
        e.acid.level  = acidLevel->load();

        e.pad.pwm      = padPwm->load();
        e.pad.subLevel = padSubLevel->load();
        e.pad.cutoff   = padCutoff->load();
        e.pad.res      = padRes->load();
        e.pad.att      = padAttack->load();
        e.pad.dec      = padDecayEnv->load();
        e.pad.sus      = padSustain->load();
        e.pad.rel      = padRelease->load();
        e.pad.lfoRate  = padLfoRate->load();
        e.pad.lfoDepth = padLfoDepth->load();
        e.pad.lfoDelay = padLfoDelay->load();
        e.pad.chorusMode = (int) padChorus->load();
        e.pad.level    = padLevel->load();

        for (int d = 0; d < 8; ++d) {
            auto& pd = e.drum.pads[d];
            pd.tune   = drumTune[d]->load();
            pd.decay  = drumDecay[d]->load();
            pd.cutoff = drumCut[d]->load();
            pd.level  = drumLvl[d]->load();
            pd.pan    = drumPan[d]->load();
            pd.choke  = (int) drumChoke[d]->load();
        }

        for (int p = 0; p < 4; ++p) {
            e.seq[p].on     = seqOn[p]->load() > 0.5f;
            e.seq[p].rate   = (int) seqRate[p]->load();
            e.seq[p].length = (int) seqLen[p]->load();
            e.seq[p].dir    = (int) seqDir[p]->load();
            e.seq[p].swing  = seqSwing[p]->load();
            e.seq[p].bank   = (int) seqBank[p]->load();
            e.mix.level[p]   = mixLvl[p]->load();
            e.mix.pan[p]     = mixPan[p]->load();
            e.mix.dlySend[p] = mixDSend[p]->load();
            e.mix.vrbSend[p] = mixRSend[p]->load();
        }

        e.dly.sync     = dlySync->load() > 0.5f;
        e.dly.div      = (int) dlyDiv->load();
        e.dly.timeS    = dlyTime->load();
        e.dly.feedback = dlyFb->load();
        e.dly.tone     = dlyTone->load();
        e.dly.mode     = (int) dlyMode->load();
        e.dly.toVerb   = dlyToVerb->load();
        e.dly.ret      = dlyRet->load();

        e.vrb.decay      = vrbDecay->load();
        e.vrb.size       = vrbSize->load();
        e.vrb.predelayMs = vrbPre->load();
        e.vrb.modDepth   = vrbMod->load();
        e.vrb.lowDamp    = vrbLoDamp->load();
        e.vrb.highDamp   = vrbHiDamp->load();
        e.vrb.shimmer    = vrbShim->load();
        e.vrb.shimInterval = (int) vrbShimInt->load();
        e.vrb.freeze     = vrbFreeze->load() > 0.5f;
        e.vrb.ret        = vrbRet->load();

        for (int p = 0; p < 4; ++p)
            e.midiCh[p] = 1 + (int) midiChRef[p]->load();
        e.midiFocus = (int) midiFocus->load();

        e.master.gain   = masterGain->load();
        e.master.hpMode = (int) masterHp->load();
        e.master.accent = masterAccent->load();
        e.freeBpm       = (double) masterBpm->load();
        return e;
    }

private:
    std::atomic<float>* bassWave{};   std::atomic<float>* bassSubWave{};
    std::atomic<float>* bassSubOct{}; std::atomic<float>* bassSubLevel{};
    std::atomic<float>* bassCutoff{}; std::atomic<float>* bassRes{};
    std::atomic<float>* bassEnvMod{}; std::atomic<float>* bassTracking{};
    std::atomic<float>* bassDecay{};  std::atomic<float>* bassAccDecay{};
    std::atomic<float>* bassAccent{}; std::atomic<float>* bassVcaDecay{};
    std::atomic<float>* bassModAtt{}; std::atomic<float>* bassModDec{};
    std::atomic<float>* bassVcfMod{}; std::atomic<float>* bassVcaMod{};
    std::atomic<float>* bassGlide{};  std::atomic<float>* bassFr{};
    std::atomic<float>* bassLevel{};
    std::atomic<float>* acidWave{};   std::atomic<float>* acidCutoff{};
    std::atomic<float>* acidRes{};    std::atomic<float>* acidEnvMod{};
    std::atomic<float>* acidDecay{};  std::atomic<float>* acidAccent{};
    std::atomic<float>* acidOctave{}; std::atomic<float>* acidLevel{};
    std::atomic<float>* padPwm{};       std::atomic<float>* padSubLevel{};
    std::atomic<float>* padCutoff{};    std::atomic<float>* padRes{};
    std::atomic<float>* padAttack{};    std::atomic<float>* padDecayEnv{};
    std::atomic<float>* padSustain{};   std::atomic<float>* padRelease{};
    std::atomic<float>* padLfoRate{};   std::atomic<float>* padLfoDepth{};
    std::atomic<float>* padLfoDelay{};  std::atomic<float>* padChorus{};
    std::atomic<float>* padLevel{};
    std::atomic<float>* drumTune[8]{};  std::atomic<float>* drumDecay[8]{};
    std::atomic<float>* drumCut[8]{};   std::atomic<float>* drumLvl[8]{};
    std::atomic<float>* drumPan[8]{};   std::atomic<float>* drumChoke[8]{};
    std::atomic<float>* seqOn[4]{};   std::atomic<float>* seqRate[4]{};
    std::atomic<float>* seqLen[4]{};  std::atomic<float>* seqDir[4]{};
    std::atomic<float>* seqSwing[4]{}; std::atomic<float>* seqBank[4]{};
    std::atomic<float>* mixLvl[4]{};  std::atomic<float>* mixPan[4]{};
    std::atomic<float>* mixDSend[4]{}; std::atomic<float>* mixRSend[4]{};
    std::atomic<float>* dlySync{};    std::atomic<float>* dlyDiv{};
    std::atomic<float>* dlyTime{};    std::atomic<float>* dlyFb{};
    std::atomic<float>* dlyTone{};    std::atomic<float>* dlyMode{};
    std::atomic<float>* dlyToVerb{};  std::atomic<float>* dlyRet{};
    std::atomic<float>* vrbDecay{};   std::atomic<float>* vrbSize{};
    std::atomic<float>* vrbPre{};     std::atomic<float>* vrbMod{};
    std::atomic<float>* vrbLoDamp{};  std::atomic<float>* vrbHiDamp{};
    std::atomic<float>* vrbShim{};    std::atomic<float>* vrbShimInt{};
    std::atomic<float>* vrbFreeze{};  std::atomic<float>* vrbRet{};
    std::atomic<float>* masterGain{}; std::atomic<float>* masterHp{};
    std::atomic<float>* masterBpm{};  std::atomic<float>* masterAccent{};
    std::atomic<float>* midiChRef[4]{}; std::atomic<float>* midiFocus{};
};

} // namespace maru::params
