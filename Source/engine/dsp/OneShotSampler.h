#pragma once
#include <cmath>
#include "SampleBuffer.h"

namespace maru {

// One-shot drum voice: cubic-Hermite playhead that STOPS at the end of the
// buffer (no loop wrap), with a 1 ms retrigger ramp (click-free restarts),
// an exponential decay envelope, and a 3 ms choke fade. The buffer pointer is
// passed into every next() call (abio pattern): a generation change mid-hit
// deactivates the voice, so a swapped buffer can never dangle.
class OneShotSampler {
public:
    static constexpr float kRetriggerRampSec = 0.001f;
    static constexpr float kChokeFadeSec = 0.003f;

    void prepare(double sampleRate) {
        sr = sampleRate;
        rampCoef = 1.0f - std::exp(-1.0f / (kRetriggerRampSec * (float) sr));
        chokeCoef = std::exp(-1.0f / (kChokeFadeSec * (float) sr));
        playing = false;
        ramp = 1.0f;
        env = 0.0f;
    }

    // decaySec <= 0 disables the envelope (sample rings out naturally)
    void trigger(const SampleBuffer* buf, double ratio_, float gain_, float decaySec) {
        if (buf == nullptr || buf->numFrames < 8) return;
        bufGeneration = buf->generation;
        pos = 0.0;
        ratio = ratio_ < 0.001 ? 0.001 : (ratio_ > 64.0 ? 64.0 : ratio_);
        gain = gain_;
        env = 1.0f;
        decCoef = decaySec > 0.0f
                ? std::exp(-1.0f / (decaySec * (float) sr))
                : 1.0f;
        ramp = 0.0f; // 1 ms attack ramp also declicks retriggers
        choking = false;
        playing = true;
    }

    void choke() {
        if (playing) choking = true;
    }

    bool active() const { return playing; }

    void next(const SampleBuffer* buf, float& outL, float& outR) {
        outL = outR = 0.0f;
        if (!playing) return;
        if (buf == nullptr || buf->numFrames < 8
            || buf->generation != bufGeneration) {
            playing = false; // buffer swapped/cleared mid-hit: stop cleanly
            return;
        }

        const int n = buf->numFrames;
        if (pos >= (double) (n - 1)) {
            playing = false;
            return;
        }

        outL = hermite(buf->left, pos, n);
        outR = hermite(buf->right, pos, n);

        ramp += (1.0f - ramp) * rampCoef;
        env *= decCoef;
        if (choking) env *= chokeCoef;
        const float a = gain * ramp * env;
        outL *= a;
        outR *= a;

        if (env < 1.0e-4f) {
            playing = false;
            return;
        }
        pos += ratio;
    }

private:
    // clamped-edge Hermite: no wrap-around reads at the boundaries
    static float hermite(const std::vector<float>& x, double p, int n) {
        int i1 = (int) p;
        if (i1 < 0) i1 = 0;
        if (i1 > n - 1) i1 = n - 1;
        const float t = (float) (p - (double) i1);
        const int i0 = i1 - 1 < 0 ? 0 : i1 - 1;
        const int i2 = i1 + 1 >= n ? n - 1 : i1 + 1;
        const int i3 = i2 + 1 >= n ? n - 1 : i2 + 1;
        const float xm1 = x[(size_t) i0], x0 = x[(size_t) i1];
        const float x1 = x[(size_t) i2], x2 = x[(size_t) i3];
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((a * t - b) * t + c) * t + x0;
    }

    double sr = 48000.0;
    double pos = 0.0, ratio = 1.0;
    uint64_t bufGeneration = 0;
    float gain = 1.0f, env = 0.0f, decCoef = 1.0f;
    float ramp = 1.0f, rampCoef = 0.05f;
    float chokeCoef = 0.99f;
    bool  playing = false, choking = false;
};

} // namespace maru
