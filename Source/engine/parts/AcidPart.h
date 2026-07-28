#pragma once
#include "../EngineParams.h"
#include "../ClockState.h"
#include "../Patterns.h"
#include "../StepClock.h"
#include "../dsp/LadderFilter.h"

namespace maru {

// Mono 303 acid line, ported from tb303/app/audio/tb303-processor.js (the
// fleet's TB-303 voice: single polyBLEP osc, ZDF tanh ladder, the accent
// circuit with resonance-dependent "wow" smoothing, fixed-rate slide RC,
// ACCENT_DECAY_CLAMP), sequenced with Blacksite step semantics on the shared
// clock. Slower and rounder than a rave 303 by default — CBL acid.
class AcidPart {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const AcidParams& p, const PartSeqParams& s) { params = p; seq = s; }
    void setPattern(const AcidPattern& p) { pattern = p; }

    void noteOn(int note, float velocity);   // ignored while seq.on
    void noteOff(int note);
    void allNotesOff();

    void render(float* L, float* R, int numSamples, const ClockState& clock);

private:
    void updateControl();
    void trigger(int note, bool accent);
    void slideTo(int note, bool accent);
    void gateOff() { gate = false; }
    void fireStep(long long stepNum, double stepBeats, double swingOff);
    int  mapDirection(long long stepNum) const;

    AcidParams    params;
    PartSeqParams seq;
    AcidPattern   pattern;
    StepClock     stepClock;

    double sr = 48000.0;

    // voice state (tb303 port)
    float phase = 0.0f;
    float noteCur = 36.0f;
    int   noteTarget = 36;
    bool  gate = false;
    float megEnv = 0.0f;  bool megAttPhase = false;
    float accEnv = 0.0f, accSm = 0.0f, accOn = 0.0f;
    float vcaEnv = 0.0f, gateSm = 0.0f;
    float sqLpState = 0.0f;
    float dcState = 0.0f;
    LadderFilter ladder;

    // sequencer state
    bool      seqWasOn = false;
    int       seqCurNote = -1;
    double    seqGateOffBeat = 0.0;
    bool      seqHoldForSlide = false;
    uint32_t  seqRng = 0x51f15eedu; // distinct fixed seed from bass
    int       seqRandStep = 0;

    // live MIDI held stack
    int  held[16] = {};
    int  heldCount = 0;
    float velCur = 1.0f;

    // control-rate derived
    int   controlCountdown = 0;
    float glideCoef = 0.0f;
    float baseCut = 200.0f, kRes = 0.0f, envOctMax = 0.0f;
    float megDecCoef = 0.0f, megAttCoef = 0.0f;
    float accSmCoef = 0.0f, accDecCoef = 0.0f;
    float vcaAttCoef = 0.0f, vcaDecCoef = 0.0f, vcaRelCoef = 0.0f;
    float sqLpCoef = 0.0f, dcCoef = 0.0f;
    float ampGain = 0.0f;
};

} // namespace maru
