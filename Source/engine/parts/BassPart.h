#pragma once
#include "../EngineParams.h"
#include "../ClockState.h"
#include "../Patterns.h"
#include "../StepClock.h"
#include "../dsp/LadderFilter.h"

namespace maru {

// Mono sub-bass part: the Avalon Bassline voice ported line-for-line from
// avalon/app/audio/avalon-processor.js (which is the TB-303 circuit plus the
// Avalon layer: sub osc pre-filter, mod envelope with bipolar depths, split
// accent/normal decays, log key tracking, FR switch), driven by a 16-step
// sequencer with per-step gate length / slide time / filter CV (avalon
// semantics) on the shared musical clock (Blacksite step derivation).
class BassPart {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const BassParams& p, const PartSeqParams& s) { params = p; seq = s; }
    void setPatterns(const BassPattern (&banks)[kNumBanks]) {
        for (int b = 0; b < kNumBanks; ++b) patterns[b] = banks[b];
    }

    void noteOn(int note, float velocity);   // ignored while seq.on
    void noteOff(int note);
    void allNotesOff();

    // Renders into L/R, OVERWRITING the buffers (part scratch, not the mix bus).
    void render(float* L, float* R, int numSamples, const ClockState& clock);

private:
    void updateControl();
    void trigger(int note, bool accent);
    void slideTo(int note, bool accent);
    void gateOff() { gate = false; }
    void fireStep(long long stepNum, double stepBeats, double swingOff);
    int  mapDirection(long long stepNum) const;

    BassParams    params;
    PartSeqParams seq;
    BassPattern   patterns[kNumBanks];
    int           activeBank = 0;
    long long     lastBar = -0x7fffffffffffffLL;
    StepClock     stepClock;

    double sr = 48000.0;

    // --- voice state (avalon port) ---
    float phase = 0.0f, subPhase = 0.0f;
    float noteCur = 36.0f;
    int   noteTarget = 36;
    bool  gate = false;
    float velCur = 1.0f;
    float megEnv = 0.0f;  bool megAttPhase = false; // main 303 envelope
    float accEnv = 0.0f, accSm = 0.0f, accOn = 0.0f;
    bool  accented = false;
    float vcaEnv = 0.0f, gateSm = 0.0f;
    float modEnv = 0.0f;  bool modAttPhase = false; // Avalon mod envelope
    float stepFiltOct = 0.0f;   // per-step filter CV, octaves
    float sqLpState = 0.0f, subLpState = 0.0f;
    float dcState = 0.0f;
    LadderFilter ladder;

    // --- sequencer state ---
    bool      seqWasOn = false;
    int       seqCurNote = -1;
    double    seqGateOffBeat = 0.0;
    bool      seqHoldForSlide = false;
    uint32_t  seqRng = 0x9e3779b9u; // fixed seed: deterministic renders
    int       seqRandStep = 0;

    // --- live MIDI held-note stack (seq off) ---
    int  held[16] = {};
    int  heldCount = 0;

    // --- control-rate derived ---
    int   controlCountdown = 0;
    float glideCoefLive = 0.0f;  // from params.glide (live MIDI)
    float glideCoefStep = 0.0f;  // from the fired step's slideT
    float glideCoef = 0.0f;      // active coefficient
    float baseCut = 200.0f, kRes = 0.0f, envOctMax = 0.0f;
    float megDecCoef = 0.0f, accSmCoef = 0.0f, accDecCoef = 0.0f;
    float vcaDecCoef = 0.0f, vcaAttCoef = 0.0f, vcaRelCoef = 0.0f;
    float megAttCoef = 0.0f;
    float modAttCoef = 0.0f, modDecCoef = 0.0f;
    float vcfDepthOct = 0.0f, vcaDepthGain = 0.0f;
    float sqLpCoef = 0.0f, dcCoef = 0.0f;
    float subLvl = 0.0f;
    float ampGain = 0.0f;
};

} // namespace maru
