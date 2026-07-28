#pragma once
#include <vector>
#include "EngineParams.h"
#include "ClockState.h"
#include "Patterns.h"
#include "parts/BassPart.h"
#include "parts/AcidPart.h"
#include "parts/DrumPart.h"
#include "parts/PadPart.h"
#include "fx/StereoDelay.h"
#include "fx/BloomReverb.h"

namespace maru {

// The groovebox core: one musical clock, four parts rendering into scratch
// buffers, a per-part mixer with delay/reverb send buses (StereoDelay +
// BloomReverb, 100% wet, with a delay->reverb feed), and a master chain.
// M1: BassPart live; acid/drums/pad slots render silence until their
// milestones.
class GrooveEngine {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void setParams(const EngineParams& p) { params = p; }
    void setPatterns(const GrooveBanks& g);
    void setPatterns(const GroovePatterns& g); // convenience: same in all banks
    // refreshed every block by the processor; null slots use the DefaultKit
    void setDrumSamples(const SampleBuffer* const bufs[8]) { drums.setSamples(bufs); }

    // midiChannel 1..4 -> bass, acid, drums, pad; other channels -> bass
    void noteOn(int midiChannel, int note, float velocity);
    void noteOff(int midiChannel, int note);
    void allNotesOff();

    void process(float* left, float* right, int numSamples, const TransportInfo& transport);

private:
    EngineParams params;
    BassPart bass;
    AcidPart acid;
    DrumPart drums;
    PadPart pad;
    StereoDelay delay;
    BloomReverb reverb;

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
