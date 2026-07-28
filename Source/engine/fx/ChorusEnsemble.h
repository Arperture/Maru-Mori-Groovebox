#pragma once
#include "DelayLine.h"
#include <cmath>

// Ported verbatim from abiogenesis/Source/fx/ChorusEnsemble.h (juno106 BBD).

namespace maru {

// Juno-style BBD chorus. mode 0 = off, 1 = I (0.513 Hz), 2 = II (0.863 Hz),
// 3 = Ensemble (both LFOs, deeper sweep — the juno's I+II button).
class ChorusEnsemble {
public:
    void prepare(double sampleRate) {
        fs = (float) sampleRate;
        buf.resize((int) (fs * 0.02f)); // 20 ms line covers base+depth
        phase1 = 0.0f;
        phase2 = 0.25f;
    }

    void process(float& xl, float& xr, int mode, float mix) {
        if (mode == 0 || mix <= 0.0001f)
            return;

        constexpr float kTwoPi = 6.283185307f;
        constexpr float kBaseMs = 3.1f, kDepthMs = 1.35f;
        constexpr float kRateI = 0.513f, kRateII = 0.863f;

        buf.push((xl + xr) * 0.5f);

        const float rate1 = mode == 2 ? kRateII : kRateI;
        phase1 += rate1 / fs;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += kRateII / fs;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        const bool both = mode == 3;
        const float m1 = std::sin(kTwoPi * phase1);
        const float m2 = both ? std::sin(kTwoPi * phase2) : m1;
        const float baseS = kBaseMs * 0.001f * fs;
        const float depthS = kDepthMs * 0.001f * fs * (both ? 1.15f : 1.0f);

        // anti-phase L/R taps: the BBD stereo trick
        const float wetL = buf.readFrac(baseS + depthS * m1 + 1.0f);
        const float wetR = buf.readFrac(baseS - depthS * m2 + 1.0f);

        const float w = 0.5f * mix;
        xl = xl * (1.0f - w) + wetL * (w + w);
        xr = xr * (1.0f - w) + wetR * (w + w);
    }

private:
    float fs = 48000.0f;
    DelayLine buf;
    float phase1 = 0.0f, phase2 = 0.25f;
};

} // namespace maru
