#pragma once
#include "DelayLine.h"

// Ported from abiogenesis/Source/fx/StereoDelay.h (aperture lineage), adapted
// for the Maru Mori send bus: callers feed the send sum in and take pure wet
// out; dry/return mixing happens in the GrooveEngine mixer. kMaxSeconds raised
// 2.0 -> 4.0 so BPM-synced divisions fit at psybient tempos (1 bar @ 60 BPM).

namespace maru {

// mode 0 = stereo, 1 = ping-pong, 2 = tape (wow + one-pole damping).
class StereoDelay {
public:
    static constexpr float kMaxSeconds = 4.0f;
    static constexpr float kMaxFeedback = 0.85f; // hard ceiling, fleet law

    void prepare(double sampleRate) {
        fs = (float) sampleRate;
        const int n = (int) (fs * (kMaxSeconds + 0.1f));
        bufL.resize(n);
        bufR.resize(n);
        timeCur = fs * 0.3f;
        lpL = lpR = 0.0f;
        wow = 0.0f;
    }

    // in: send-bus sample (already tanh-guarded); out: wet only
    void process(float inL, float inR, float& outL, float& outR,
                 int mode, float timeS, float feedback, float tone) {
        constexpr float kTwoPi = 6.283185307f;

        float target = timeS * fs;
        const float maxT = (float) (bufL.size() - 4);
        if (target > maxT) target = maxT;
        if (target < 16.0f) target = 16.0f;
        timeCur += (target - timeCur) * 0.0004f; // slow glide: repitch, no clicks

        float readT = timeCur;
        if (mode == 2) {
            wow += 0.9f / fs;
            if (wow >= 1.0f) wow -= 1.0f;
            readT *= 1.0f + 0.0022f * std::sin(kTwoPi * wow);
        }

        float dl = bufL.readFrac(readT);
        float dr = bufR.readFrac(readT);

        // feedback tone: one-pole LP, tone=1 bright (bypass-ish), 0 dark.
        // Tape mode darkens harder (the original 0.25 head coefficient).
        const float lpCoef = mode == 2 ? 0.25f : 0.15f + 0.85f * tone;
        lpL += (dl - lpL) * lpCoef;
        lpR += (dr - lpR) * lpCoef;
        dl = lpL;
        dr = lpR;

        const float fb = feedback > kMaxFeedback ? kMaxFeedback : feedback;
        if (mode == 1) {
            // ping-pong: input enters the left line, lines cross-feed
            bufL.push((inL + inR) * 0.5f + dr * fb);
            bufR.push(dl * fb);
        } else {
            bufL.push(inL + dl * fb);
            bufR.push(inR + dr * fb);
        }

        outL = dl;
        outR = dr;
    }

private:
    float fs = 48000.0f;
    DelayLine bufL, bufR;
    float timeCur = 14400.0f;
    float lpL = 0.0f, lpR = 0.0f;
    float wow = 0.0f;
};

} // namespace maru
