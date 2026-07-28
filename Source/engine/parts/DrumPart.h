#pragma once
#include "../EngineParams.h"
#include "../ClockState.h"
#include "../Patterns.h"
#include "../StepClock.h"
#include "../dsp/OneShotSampler.h"
#include "../dsp/DefaultKit.h"

namespace maru {

// 8-pad one-shot drum machine: sample slots (user files via the JUCE-side
// stores, DefaultKit fallback), a 16-step x 8-lane grid sequencer on the
// shared clock, choke groups, per-pad tune/decay/filter/level/pan.
// Live MIDI (ch 3, notes 36..43) always layers over the grid — drums are
// polyphonic, the mono-part takeover rule doesn't apply.
class DrumPart {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const DrumParams& p, const PartSeqParams& s) { params = p; seq = s; }
    void setGrids(const DrumGrid (&banks)[kNumBanks]) {
        for (int b = 0; b < kNumBanks; ++b) grids[b] = banks[b];
    }
    // refreshed every block by the processor; null slot -> DefaultKit pad
    void setSamples(const SampleBuffer* const bufs[8]);

    void trigger(int pad, float velocity); // live path
    void allNotesOff();

    void render(float* L, float* R, int numSamples, const ClockState& clock);

private:
    void fireStep(long long stepNum);
    void firePad(int pad, float velocity);
    int  mapDirection(long long stepNum) const;
    const SampleBuffer* bufFor(int pad) const;

    DrumParams    params;
    PartSeqParams seq;
    DrumGrid      grids[kNumBanks];
    int           activeBank = 0;
    long long     lastBar = -0x7fffffffffffffLL;
    StepClock     stepClock;
    DefaultKit    defaultKit;

    const SampleBuffer* ext[8] = {};
    OneShotSampler voice[8];

    double sr = 48000.0;
    bool   seqWasOn = false;
    uint32_t seqRng = 0x00d80909u;
    int    seqRandStep = 0;

    // control-rate derived per pad
    int   controlCountdown = 0;
    float lpCoef[8] = {};
    float lpL[8] = {}, lpR[8] = {};
    float panL[8] = {}, panR[8] = {};
};

} // namespace maru
