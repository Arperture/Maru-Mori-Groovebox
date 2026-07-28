#pragma once
#include "DelayLine.h"

// Ported verbatim from abiogenesis/Source/fx/ShimmerVoice.h.

namespace maru {

// Arbitrary-ratio granular pitch shifter: two read heads racing (or trailing)
// the write head with Hann-crossfaded taps. Made for reverb-tank shimmer
// feedback.
class ShimmerVoice {
public:
    void prepare(double sampleRate, float windowSeconds = 0.12f) {
        fs = (float) sampleRate;
        window = fs * windowSeconds;
        buf.resize((int) window + 8);
        ph = 0.0f;
    }

    void setRatio(float r) { ratio = r; }

    float process(float x) {
        constexpr float kPi = 3.14159265358979f;
        buf.push(x);

        if (std::fabs(ratio - 1.0f) < 1.0e-4f)
            return buf.readFrac(window * 0.5f); // unity: plain delay

        ph += (ratio - 1.0f) / window;
        ph -= std::floor(ph);
        float ph2 = ph + 0.5f;
        if (ph2 >= 1.0f) ph2 -= 1.0f;

        // delay = window*(1-ph) gives read-rate = ratio for both directions:
        // ph rises for up-shift (delay shrinks), falls for down-shift (grows)
        const float d1 = window * (1.0f - ph);
        const float d2 = window * (1.0f - ph2);
        const float g1 = std::sin(kPi * ph);
        const float g2 = std::sin(kPi * ph2);
        return buf.readFrac(d1 + 2.0f) * g1 * g1 + buf.readFrac(d2 + 2.0f) * g2 * g2;
    }

private:
    float fs = 48000.0f;
    float window = 5760.0f;
    float ph = 0.0f;
    float ratio = 2.0f;
    DelayLine buf;
};

} // namespace maru
