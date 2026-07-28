#pragma once

namespace maru {

// Host transport, read once per block from the JUCE playhead into plain data
// so the engine never touches JUCE types.
struct TransportInfo {
    double bpm = 120.0;
    double ppq = 0.0;
    bool   playing = false;
};

// ---------------------------------------------------------------------------
// EngineParams: a plain struct of floats/ints/bools snapshotted once per block
// from the APVTS atomics (ParamRefs::snapshot() in Params.h). Defaults here
// mirror the APVTS defaults in Params.h — keep them in sync by hand.
// ---------------------------------------------------------------------------

// Avalon-topology mono sub bass (see parts/BassPart.cpp for the port notes).
struct BassParams {
    int   wave      = 0;     // main osc: 0 saw, 1 off (sub solo), 2 square
    int   subWave   = 0;     // 0 saw, 1 triangle, 2 square
    int   subOct    = 1;     // 1 = -1 oct, 2 = -2 oct
    float subLevel  = 0.7f;
    float cutoff    = 0.4f;  // 0..1 -> 80..4200 Hz exponential
    float res       = 0.3f;
    float envMod    = 0.45f; // 0..1, MEG -> cutoff (4.3 oct max)
    float tracking  = 0.3f;  // 0..1, log key tracking around D#2
    float envDecay  = 0.5f;  // MEG decay, unaccented (0.2..2 s log)
    float accDecay  = 0.35f; // MEG decay, accented
    float accent    = 0.6f;
    float vcaDecay  = 0.75f; // 0.006..4 s log
    float modAtt    = 0.25f; // mod env attack, 0.9 ms..6 s log
    float modDec    = 0.4f;  // mod env decay, 1.7 ms..10 s log
    float vcfModDepth = 0.0f; // -1..1, mod env -> cutoff (+-3 oct)
    float vcaModDepth = 0.0f; // -1..1; negative opens the VCA (drone mode)
    float glide     = 0.1f;  // live-MIDI slide time seconds (seq steps override)
    int   fr        = 1;     // 0 = 70 Hz (303), 1 = 18 Hz full range (CBL sub)
    float level     = 0.8f;
};

// TB-303 acid line (tb303 port; constants live in AcidPart.cpp).
struct AcidParams {
    int   wave   = 0;     // 0 saw, 1 square
    float cutoff = 0.35f; // 0..1 -> 80..4200 Hz exponential
    float res    = 0.5f;
    float envMod = 0.5f;
    float decay  = 0.4f;
    float accent = 0.6f;
    int   octave = 0;     // -1 / 0 / +1 pattern transpose
    float level  = 0.8f;
};

// One-shot sample drums, 8 pads.
struct DrumPadParams {
    float tune   = 0.0f;  // semitones -24..+24
    float decay  = 1.0f;  // 0..1 log 10 ms..4 s; >= 0.98 = ring out
    float cutoff = 1.0f;  // pad LP, 1.0 = open
    float level  = 0.8f;
    float pan    = 0.0f;
    int   choke  = 0;     // 0 = off, 1..4 = choke group
};
struct DrumParams {
    float accentScale = 1.0f; // set by the engine from master.accent
    DrumPadParams pads[8]; // kick, snare, clap, ch, oh, tom, rim, shaker
    DrumParams() {         // mirror the APVTS defaults in Params.h
        constexpr float kPan[8]  = { 0.0f, 0.0f, 0.15f, -0.2f, -0.2f, 0.3f, -0.35f, 0.4f };
        constexpr int   kChoke[8] = { 0, 0, 0, 1, 1, 0, 0, 0 }; // CH+OH share group 1
        for (int i = 0; i < 8; ++i) {
            pads[i].pan = kPan[i];
            pads[i].choke = kChoke[i];
        }
    }
};

// Juno-flavored poly pad (8 voices, BBD chorus insert).
struct PadParams {
    float pwm      = 0.4f;   // pulse-width mod depth (LFO breathes it)
    float subLevel = 0.4f;
    float cutoff   = 0.55f;  // 0..1 -> 60 Hz..12 kHz
    float res      = 0.15f;
    float att      = 0.55f;  // ADSR, norm 0..1 log 5 ms..12 s
    float dec      = 0.5f;
    float sus      = 0.7f;
    float rel      = 0.6f;
    float lfoRate  = 0.3f;   // 0.1..8 Hz log
    float lfoDepth = 0.15f;
    float lfoDelay = 0.5f;   // juno ramp-in time
    int   chorusMode = 3;    // 0 off, 1 I, 2 II, 3 Ensemble
    float level    = 0.7f;
};

struct PartSeqParams {
    bool  on     = false;
    int   rate   = 0;      // index into kSeqRateBeats
    int   length = 16;
    int   dir    = 0;      // 0 FWD, 1 REV, 2 PENDULUM, 3 RANDOM
    float swing  = 0.0f;
    int   bank   = 0;      // requested pattern bank A-D; applied at bar start
};

struct MixerParams {
    float level[4]   = { 0.8f, 0.8f, 0.8f, 0.8f }; // bass, acid, drums, pad
    float pan[4]     = { 0.0f, 0.0f, 0.0f, 0.0f }; // -1..1
    float dlySend[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float vrbSend[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct DelayParams {
    bool  sync     = true;
    int   div      = 4;     // index into kDelayDivBeats
    float timeS    = 0.5f;  // free-run seconds when !sync
    float feedback = 0.45f;
    float tone     = 0.5f;
    int   mode     = 1;     // 0 stereo, 1 ping-pong, 2 tape
    float toVerb   = 0.0f;  // delay output bled into the reverb send
    float ret      = 0.8f;  // return level to master
};

struct VerbParams {
    float decay      = 0.55f;
    float size       = 0.75f;
    float predelayMs = 20.0f;
    float modDepth   = 0.4f;
    float modRate    = 0.35f; // fixed pre-M-final (no APVTS param yet)
    float lowDamp    = 0.15f;
    float highDamp   = 0.35f;
    float shimmer    = 0.0f;
    int   shimInterval = 0;
    bool  freeze     = false;
    float ret        = 0.8f;
};

struct MasterParams {
    float gain   = 0.9f;  // linear
    int   hpMode = 0;     // 0 = 18 Hz full-range, 1 = 70 Hz
    float accent = 0.5f;  // global accent: 0.5 neutral, scales all parts 0..2x
};

struct EngineParams {
    BassParams    bass;
    AcidParams    acid;
    DrumParams    drum;
    PadParams     pad;
    PartSeqParams seq[4];  // bass, acid, drums, pad
    int midiCh[4] = { 1, 2, 3, 4 }; // per-part MIDI channel (1..16)
    int midiFocus = 0;              // 0 = channel routing; 1..4 = that part
                                    // takes ALL incoming MIDI (controller focus)
    MixerParams   mix;
    DelayParams   dly;
    VerbParams    vrb;
    MasterParams  master;
    double        freeBpm = 100.0; // internal clock when host isn't playing
};

// Shared musical tables (indices referenced by params above)
// 1/16, 1/8, 1/16T, 1/8T, 1/4, 1/2, 1/1 — in beats
inline constexpr double kSeqRateBeats[7] = { 0.25, 0.5, 1.0 / 6.0, 1.0 / 3.0, 1.0, 2.0, 4.0 };
// 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2, 1/2., 1/1 — in beats
inline constexpr double kDelayDivBeats[9] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 };

} // namespace maru
