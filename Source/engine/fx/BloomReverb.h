#pragma once
#include "DelayLine.h"
#include "ShimmerVoice.h"
#include "../../Tuning.h"

// Ported from abiogenesis/Source/fx/BloomReverb.h (the flagship fleet reverb),
// adapted for the Maru Mori send bus: pure-wet output, dry/return mixing in
// the GrooveEngine mixer. This is THE Carbon Based Lifeforms tail.

namespace maru {

// "Bloom" — 8-line FDN with Householder mixing, input diffusion, slow tank
// modulation, separate low/high damping, decay to 45 s, lossless freeze
// (infinite hold), and a dual-voice shimmer feedback path with selectable
// interval. BigSky / Valhalla Shimmer are the north stars; tuned by ear.
class BloomReverb {
public:
    static constexpr int kLines = 8;

    struct Params {
        float decay = 0.55f;     // 0..1 -> T60 0.4..45 s (log)
        float size = 0.75f;      // 0..1 -> line-length scale 0.25..1.6
        float predelayMs = 20.0f;
        float modDepth = 0.4f;   // 0..1
        float modRate = 0.35f;   // 0..1 -> 0.25x..4x base rates
        float lowDamp = 0.15f;   // 0..1 tail low-cut strength
        float highDamp = 0.35f;  // 0..1 tail high-cut strength
        float shimmer = 0.0f;    // 0..1 feedback shimmer amount
        int   shimInterval = 0;  // 0:+12 1:+7 2:+5 3:-12 4:+12&+7
        bool  freeze = false;
    };

    void prepare(double sampleRate) {
        fs = (float) sampleRate;
        const float k = fs / 48000.0f;

        pre.resize((int) (fs * 0.26f) + 8);
        static constexpr int apLen[4] = { 113, 229, 349, 461 };
        static constexpr float apGain[4] = { 0.70f, 0.70f, 0.625f, 0.625f };
        for (int i = 0; i < 4; ++i)
            ap[i].resize((int) std::lround((float) apLen[i] * k), apGain[i]);

        for (int i = 0; i < kLines; ++i) {
            baseLen[i] = (float) kSeedLengths[i] * k;
            // headroom: max size 1.6x + mod excursion + interp guard
            line[i].resize((int) (baseLen[i] * 1.6f) + 64);
            lpState[i] = hpState[i] = 0.0f;
        }
        tankEnv = 0.0f;
        shifterA.prepare(sampleRate);
        shifterB.prepare(sampleRate);
        inLp = 0.0f;
        sizeCur = 0.75f;
        freezeRamp = 0.0f;
        for (int i = 0; i < 4; ++i) modPhase[i] = 0.25f * (float) i;
        shimState = 0.0f;
    }

    // in: send-bus sample (already tanh-guarded); out: wet only
    void process(float inL, float inR, float& outL, float& outR, const Params& p) {
        // --- slow-moving controls ---
        const float sizeTarget = 0.25f + 1.35f * p.size;
        sizeCur += (sizeTarget - sizeCur) * (1.0f - std::exp(-1.0f / (0.08f * fs)));

        const float frzTarget = p.freeze ? 1.0f : 0.0f;
        freezeRamp += (frzTarget - freezeRamp) * (1.0f - std::exp(-1.0f / (0.05f * fs)));

        // T60: 0.4s .. 45s (log map); freeze forces lossless recirculation
        const float t60 = 0.4f * std::pow(45.0f / 0.4f, p.decay);

        // damping coefficients (bypassed as freeze engages)
        const float dampAmt = 1.0f - freezeRamp;
        const float lpHz = 16000.0f * std::exp2(-3.0f * p.highDamp);
        const float lpCoef = 1.0f - std::exp(-6.2831853f * lpHz / fs);
        const float hpHz = 20.0f * std::exp2(4.3f * p.lowDamp); // 20..400 Hz
        const float hpCoef = 1.0f - std::exp(-6.2831853f * hpHz / fs);

        // --- input path ---
        float v = (inL + inR) * 0.5f;
        pre.push(v);
        float preOff = p.predelayMs * 0.001f * fs;
        if (preOff > (float) (pre.size() - 2)) preOff = (float) (pre.size() - 2);
        v = pre.readFrac(preOff < 1.0f ? 1.0f : preOff);
        inLp += (v - inLp) * 0.55f;
        v = inLp;
        for (auto& a : ap)
            v = a.process(v);

        // shimmer feedback joins the tank input; input fades out during freeze
        v = v * (1.0f - freezeRamp) + std::tanh(shimState);

        // --- read the 8 modulated lines ---
        const float modBase = p.modDepth * 16.0f * (1.0f - 0.6f * freezeRamp);
        const float rateScale = 0.25f * std::exp2(4.0f * p.modRate); // 0.25..4
        static constexpr float kModHz[4] = { 0.13f, 0.19f, 0.23f, 0.31f };
        for (int m = 0; m < 4; ++m) {
            modPhase[m] += kModHz[m] * rateScale / fs;
            if (modPhase[m] >= 1.0f) modPhase[m] -= 1.0f;
        }

        float out[kLines];
        for (int i = 0; i < kLines; ++i) {
            float readLen = baseLen[i] * sizeCur;
            if ((i & 1) == 0) // lines 0/2/4/6 get slow chorusing
                readLen += modBase * std::sin(6.2831853f * modPhase[i >> 1]);
            const float maxLen = (float) (line[i].size() - 2);
            out[i] = line[i].readFrac(readLen > maxLen ? maxLen : readLen);
        }

        // --- Householder mixing: y_i = x_i - (sum)/4 ---
        float sum = 0.0f;
        for (float o : out) sum += o;
        const float quarter = sum * 0.25f;

        // slow tank-energy tracker: governs shimmer injection into a
        // lossless (frozen) tank, which would otherwise grow forever
        tankEnv += (std::fabs(sum) * 0.125f - tankEnv) * 0.0002f;

        // --- write back with decay + damping ---
        for (int i = 0; i < kLines; ++i) {
            float y = out[i] - quarter;

            // per-line decay gain; exactly 1 while frozen
            const float gDecay = std::pow(10.0f, -3.0f * (baseLen[i] * sizeCur) / (t60 * fs));
            const float g = gDecay + (1.0f - gDecay) * freezeRamp;

            // damping shelves, faded out while frozen
            lpState[i] += (y - lpState[i]) * lpCoef;
            float damped = lpState[i];
            hpState[i] += (damped - hpState[i]) * hpCoef;
            damped = damped - hpState[i] * (0.55f * p.lowDamp);
            y = y + (damped - y) * dampAmt;

            line[i].push(y * g + (i == 0 ? v : i == 4 ? v * 0.7f : 0.0f));
        }

        // --- shimmer path: tap two lines, dual pitch shift ---
        static constexpr float kRatios[4] = { 2.0f, 1.4983f, 1.3348f, 0.5f };
        const float tap = (out[1] + out[5]) * 0.25f;
        float shifted;
        if (p.shimInterval == 4) { // +12 & +7 dual
            shifterA.setRatio(2.0f);
            shifterB.setRatio(1.4983f);
            shifted = (shifterA.process(tap) + shifterB.process(tap)) * 0.5f;
        } else {
            const int idx = p.shimInterval < 0 ? 0 : p.shimInterval > 3 ? 3 : p.shimInterval;
            shifterA.setRatio(kRatios[idx]);
            shifted = shifterA.process(tap);
            shifterB.process(tap); // keep B's state warm for mode switches
        }
        // attenuate shimmer into a lossless (frozen) tank — it diverges
        // otherwise; the tankEnv governor caps the equilibrium level
        const float governor = 1.0f / (1.0f + freezeRamp * tankEnv * tankEnv * 4.0f);
        shimState = shifted * p.shimmer * tune::kBloomShimCeil
                  * (1.0f - 0.5f * freezeRamp) * governor;

        // --- stereo output taps (pure wet) ---
        outL = (out[0] + out[3] - out[6]) * 0.5f;
        outR = (out[2] + out[5] - out[0]) * 0.5f;
    }

private:
    // mutually prime seed lengths @48k, ~30-79 ms
    static constexpr int kSeedLengths[kLines] =
        { 1427, 1783, 1973, 2099, 2557, 2969, 3343, 3803 };

    float fs = 48000.0f;
    DelayLine pre;
    Allpass ap[4];
    DelayLine line[kLines];
    float baseLen[kLines] = {};
    float lpState[kLines] = {}, hpState[kLines] = {};
    ShimmerVoice shifterA, shifterB;
    float tankEnv = 0.0f;
    float modPhase[4] = {};
    float inLp = 0.0f;
    float sizeCur = 0.75f;
    float freezeRamp = 0.0f;
    float shimState = 0.0f;
};

} // namespace maru
