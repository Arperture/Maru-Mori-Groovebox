#pragma once
#include "../EngineParams.h"
#include "../ClockState.h"
#include "../Patterns.h"
#include "../StepClock.h"
#include "../fx/ChorusEnsemble.h"
#include "PadVoice.h"
#include "../../Tuning.h"

namespace maru {

// Polyphonic pad part: 8 PadVoices with the abio allocator (idle ->
// quietest-releasing -> oldest, retrigger-from-current-level), a part-global
// LFO with the juno delay ramp, BBD chorus insert, and a chord-slot step
// sequencer (each gated step triggers one of 4 chords; tie holds).
class PadPart {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const PadParams& p, const PartSeqParams& s) { params = p; seq = s; }
    void setPattern(const PadPattern& p) { pattern = p; }

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    void render(float* L, float* R, int numSamples, const ClockState& clock);

private:
    PadVoice* allocateVoice();
    void fireStep(long long stepNum);
    int  mapDirection(long long stepNum) const;
    void triggerChord(int chordIx);
    void releaseChord();
    void noteOnInternal(int note, float vel);
    void noteOffInternal(int note);

    PadParams     params;
    PartSeqParams seq;
    PadPattern    pattern;
    StepClock     stepClock;
    ChorusEnsemble chorus;

    PadVoice voices[tune::kPadVoices];
    uint32_t ageCounter = 0;
    double sr = 48000.0;

    bool seqWasOn = false;
    int  curChord = -1;      // chord currently held by the sequencer
    uint32_t seqRng = 0x9adface1u;
    int  seqRandStep = 0;

    // part-global LFO with delay ramp (resets when all voices go silent)
    float lfoPhase = 0.0f;
    float lfoRamp = 0.0f;

    int controlCountdown = 0;
    float lfoV = 0.0f;
};

} // namespace maru
