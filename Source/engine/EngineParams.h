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

struct BassParams {
    int   wave      = 0;     // 0 saw, 1 off (sub solo), 2 square
    int   subWave   = 0;     // 0 sine, 1 triangle, 2 square
    int   subOct    = 1;     // 1 = -1 oct, 2 = -2 oct
    float subLevel  = 0.7f;
    float cutoff    = 0.35f; // normalized 0..1 -> kBassCutoffMin..Max exponential
    float res       = 0.25f;
    float envMod    = 0.45f; // bipolar -1..1, mod env -> cutoff
    float modAtt    = 0.01f; // normalized 0..1, log-mapped seconds
    float modDec    = 0.35f;
    float vcaModDepth = 0.0f; // bipolar; negative = drone mode (opens VCA)
    float accent    = 0.6f;
    float decayNorm = 0.4f;  // VCA decay, unaccented
    float decayAcc  = 0.2f;  // VCA decay, accented
    float glide     = 0.055f; // seconds, slide time (per-step override at M1)
    float level     = 0.8f;  // part pre-mixer trim
};

struct PartSeqParams {
    bool  on     = false;
    int   rate   = 0;      // index into kSeqRateBeats
    int   length = 16;
    int   dir    = 0;      // 0 FWD, 1 REV, 2 PING-PONG, 3 RANDOM
    float swing  = 0.0f;
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
    bool  pingPong = true;
    float toVerb   = 0.0f;  // delay output bled into the reverb send
    float ret      = 0.8f;  // return level to master
};

struct VerbParams {
    float decay      = 0.5f;
    float size       = 0.7f;
    float predelayMs = 20.0f;
    float modDepth   = 0.3f;
    float lowDamp    = 0.2f;
    float highDamp   = 0.4f;
    float shimmer    = 0.0f;
    int   shimInterval = 0;
    bool  freeze     = false;
    float ret        = 0.8f;
};

struct MasterParams {
    float gain   = 0.9f;  // linear
    int   hpMode = 0;     // 0 = 18 Hz full-range, 1 = 70 Hz
};

struct EngineParams {
    BassParams    bass;
    PartSeqParams seq[4];  // bass, acid, drums, pad
    MixerParams   mix;
    DelayParams   dly;
    VerbParams    vrb;
    MasterParams  master;
    double        freeBpm = 100.0; // internal clock when host isn't playing
};

} // namespace maru
