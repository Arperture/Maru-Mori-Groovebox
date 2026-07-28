#pragma once
#include <vector>
#include "EngineParams.h"
#include "ClockState.h"
#include "Patterns.h"
#include "parts/BassPart.h"

namespace maru {

// The groovebox core: one musical clock, four parts rendering into scratch
// buffers, a per-part mixer with delay/reverb send buses, and a master chain.
// M0: BassPart live, other part slots render silence; send FX are summed but
// not yet processed (StereoDelay + BloomReverb land at M1).
class GrooveEngine {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const EngineParams& p) { params = p; }
    void setPatterns(const GroovePatterns& g);

    // midiChannel 1..4 -> bass, acid, drums, pad; other channels -> bass
    void noteOn(int midiChannel, int note, float velocity);
    void noteOff(int midiChannel, int note);
    void allNotesOff();

    void process(float* left, float* right, int numSamples, const TransportInfo& transport);

private:
    EngineParams params;
    BassPart bass;

    double sr = 48000.0;

    // musical clock (Blacksite semantics)
    double musicalPos  = 0.0;
    double lastSeenPpq = -1.0e18;

    // mixer smoothing state
    float smLevel[4] = {}, smPan[4] = {};

    // master chain state
    float hpL = 0.0f, hpR = 0.0f;

    // scratch buffers, sized in prepare (never on the audio thread)
    std::vector<float> partL[4], partR[4];
    std::vector<float> sendDL, sendDR, sendRL, sendRR;
};

} // namespace maru
