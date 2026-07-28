#pragma once
#include <cmath>
#include "SampleBuffer.h"

namespace maru {

// Built-in 808-ish kit, synthesized deterministically at prepare time (message
// thread / RenderTest setup — allocation is fine there). Zero shipped assets:
// the drum part always makes sound, and RenderTest needs no file I/O. Each pad
// falls back to these when its SampleStore slot is empty.
// Pads: 0 kick, 1 snare, 2 clap, 3 closed hat, 4 open hat, 5 tom, 6 rim,
// 7 shaker. Soft and dubby by design — CBL drums, not gabber.
class DefaultKit {
public:
    static constexpr int kPads = 8;

    void prepare(double sampleRate) {
        sr = (float) sampleRate;
        rng = 0x1c2b3a4du; // fixed seed: identical kit every prepare
        synthKick(pads[0]);
        synthSnare(pads[1]);
        synthClap(pads[2]);
        synthHat(pads[3], 0.025f, 0.08f);  // closed
        synthHat(pads[4], 0.18f, 0.60f);   // open
        synthTom(pads[5]);
        synthRim(pads[6]);
        synthShaker(pads[7]);
        for (int i = 0; i < kPads; ++i) {
            pads[i].sourceRate = sr;
            pads[i].numFrames = (int) pads[i].left.size();
            pads[i].generation = 0xD00Dull + (uint64_t) i; // distinct from store gens
            pads[i].right = pads[i].left;
        }
    }

    const SampleBuffer* pad(int i) const {
        return (i >= 0 && i < kPads) ? &pads[i] : nullptr;
    }

private:
    float sr = 48000.0f;
    uint32_t rng = 1;
    SampleBuffer pads[kPads];

    float noise() { // xorshift white noise in -1..1, deterministic
        uint32_t x = rng;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        rng = x;
        return (float) x / 2147483648.0f - 1.0f;
    }

    void alloc(SampleBuffer& b, float seconds) {
        b.left.assign((size_t) (seconds * sr), 0.0f);
    }

    void synthKick(SampleBuffer& b) {
        alloc(b, 0.55f);
        float phase = 0.0f;
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            const float f = 45.0f + 115.0f * std::exp(-t / 0.045f); // pitch drop
            phase += f / sr;
            const float amp = std::exp(-t / 0.20f);
            const float click = 0.5f * std::exp(-t / 0.004f) * noise();
            b.left[i] = std::tanh((std::sin(6.2831853f * phase) * amp + click) * 1.4f) * 0.95f;
        }
    }

    void synthSnare(SampleBuffer& b) {
        alloc(b, 0.28f);
        float p1 = 0.0f, p2 = 0.0f, hp = 0.0f;
        const float hpC = 1.0f - std::exp(-6.2831853f * 900.0f / sr);
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            p1 += 186.0f / sr; p2 += 278.0f / sr;
            const float tone = (std::sin(6.2831853f * p1) + 0.6f * std::sin(6.2831853f * p2))
                             * std::exp(-t / 0.055f) * 0.5f;
            const float w = noise();
            hp += (w - hp) * hpC;
            const float snap = (w - hp) * std::exp(-t / 0.085f) * 0.55f;
            b.left[i] = std::tanh(tone + snap) * 0.85f;
        }
    }

    void synthClap(SampleBuffer& b) {
        alloc(b, 0.30f);
        float hp = 0.0f, lp = 0.0f;
        const float hpC = 1.0f - std::exp(-6.2831853f * 700.0f / sr);
        const float lpC = 1.0f - std::exp(-6.2831853f * 4200.0f / sr);
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            // three pre-bursts then the body — the classic clap envelope
            float env = std::exp(-t / 0.11f) * 0.7f;
            if (t < 0.010f)      env = std::exp(-std::fmod(t, 0.010f) / 0.004f);
            else if (t < 0.021f) env = std::exp(-std::fmod(t - 0.010f, 0.011f) / 0.004f);
            else if (t < 0.033f) env = std::exp(-(t - 0.021f) / 0.005f) * 0.9f + 0.55f;
            const float w = noise();
            hp += (w - hp) * hpC;
            lp += ((w - hp) - lp) * lpC;
            b.left[i] = std::tanh(lp * env * 2.2f) * 0.8f;
        }
    }

    void synthHat(SampleBuffer& b, float tau, float seconds) {
        alloc(b, seconds);
        // six inharmonic squares, 808-style metal cluster
        static constexpr float kF[6] = { 3011.0f, 3733.0f, 4501.0f, 5213.0f, 6007.0f, 6857.0f };
        float ph[6] = {};
        float hp = 0.0f;
        const float hpC = 1.0f - std::exp(-6.2831853f * 5800.0f / sr);
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            float m = 0.0f;
            for (int o = 0; o < 6; ++o) {
                ph[o] += kF[o] / sr;
                if (ph[o] >= 1.0f) ph[o] -= 1.0f;
                m += ph[o] < 0.5f ? 1.0f : -1.0f;
            }
            m = m / 6.0f + noise() * 0.35f;
            hp += (m - hp) * hpC;
            b.left[i] = (m - hp) * std::exp(-t / tau) * 0.6f;
        }
    }

    void synthTom(SampleBuffer& b) {
        alloc(b, 0.38f);
        float phase = 0.0f;
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            const float f = 82.0f + 60.0f * std::exp(-t / 0.06f);
            phase += f / sr;
            b.left[i] = std::tanh(std::sin(6.2831853f * phase)
                                  * std::exp(-t / 0.16f) * 1.2f) * 0.85f;
        }
    }

    void synthRim(SampleBuffer& b) {
        alloc(b, 0.07f);
        float phase = 0.0f;
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            phase += 1720.0f / sr;
            const float ring = std::sin(6.2831853f * phase) * std::exp(-t / 0.009f);
            const float click = noise() * std::exp(-t / 0.0015f) * 0.6f;
            b.left[i] = std::tanh((ring + click) * 1.6f) * 0.7f;
        }
    }

    void synthShaker(SampleBuffer& b) {
        alloc(b, 0.14f);
        float hp = 0.0f;
        const float hpC = 1.0f - std::exp(-6.2831853f * 7800.0f / sr);
        for (size_t i = 0; i < b.left.size(); ++i) {
            const float t = (float) i / sr;
            const float attack = 1.0f - std::exp(-t / 0.012f);
            const float w = noise();
            hp += (w - hp) * hpC;
            b.left[i] = (w - hp) * attack * std::exp(-t / 0.045f) * 0.55f;
        }
    }
};

} // namespace maru
