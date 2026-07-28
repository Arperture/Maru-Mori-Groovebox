#pragma once
#include "../EngineParams.h"
#include "../ClockState.h"
#include "../Patterns.h"
#include "../StepClock.h"
#include "../dsp/LadderFilter.h"

namespace maru {

// Mono sub-bass part, avalon topology: main osc (saw / off / square) + sub osc
// (sine / tri / square at -1 or -2 oct) mixed PRE-filter into one tanh ladder.
// M0 ships the voice skeleton (saw + sub + ladder + exp envelopes); the full
// avalon port (mod env depths, drone mode, key tracking, per-step sequencer
// fields) lands at M1.
class BassPart {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const BassParams& p, const PartSeqParams& s) { params = p; seq = s; }
    void setPattern(const BassPattern& p) { pattern = p; }

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    // Renders into L/R, OVERWRITING the buffers (part scratch, not the mix bus).
    void render(float* L, float* R, int numSamples, const ClockState& clock);

private:
    void updateControl();

    BassParams    params;
    PartSeqParams seq;
    BassPattern   pattern;
    StepClock     stepClock;

    double sr = 48000.0;

    // voice state
    float noteCur = 36.0f, noteTarget = 36.0f;
    bool  gate = false;
    float velCur = 1.0f;
    float vcaEnv = 0.0f;   // exp AD amp envelope
    float modEnv = 0.0f;   // exp AD modulation envelope (filter at M0)
    bool  modEnvRising = false;
    float gateSm = 0.0f;   // smoothed gate for click-free on/off

    LadderFilter ladder;
    float oscPhase = 0.0f, subPhase = 0.0f;

    // control-rate derived
    int   controlCountdown = 0;
    float dt = 0.0f, subDt = 0.0f;
    float glideCoef = 0.0f, gateCoef = 0.0f;
    float vcaAttCoef = 0.0f, vcaDecCoef = 0.0f;
    float modAttCoef = 0.0f, modDecCoef = 0.0f;
    float g = 0.0f, k = 0.0f, resMakeup = 1.0f;
    float ampGain = 0.0f;
};

} // namespace maru
