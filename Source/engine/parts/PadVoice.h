#pragma once
#include <cmath>
#include "../EngineParams.h"
#include "../dsp/OscUtils.h"
#include "../dsp/LadderFilter.h"
#include "../dsp/AmbientADSR.h"
#include "../../Tuning.h"

namespace maru {

// One juno-flavored pad voice: saw + PWM pulse + square sub (-1 oct) into the
// ZDF ladder, AmbientADSR on the VCA (slow attacks, retrigger-from-level).
// Mono out — the part's BBD chorus makes the stereo.
class PadVoice {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        env.prepare(sampleRate);
        ladder.reset();
        phase = subPhase = 0.0f;
    }

    void noteOn(int n, float vel, uint32_t ageStamp) {
        note = n;
        velocity = vel;
        age = ageStamp;
        if (env.isIdle()) {          // fresh voice: reset filter, no click risk
            ladder.reset();
            phase = subPhase = 0.0f;
        }
        env.noteOn();                // steal-safe: attack from current level
    }

    void noteOff() { env.noteOff(); }
    void kill()    { env.kill(); }

    bool isIdle()      const { return env.isIdle(); }
    bool isReleasing() const { return env.isReleasing(); }
    float envLevel()   const { return env.value(); }
    int  currentNote() const { return note; }
    uint32_t voiceAge() const { return age; }

    void setTimes(float a, float d, float s, float r) { env.setTimes(a, d, s, r); }

    // control-rate: cache pitch/filter coefficients
    void updateControl(const PadParams& p, float lfoV) {
        const float vib = 1.0f + lfoV * 0.006f; // gentle vibrato, ~10 cents max
        const float hz = midiToHz((float) note) * vib;
        dt = hz / (float) sr;
        subDt = dt * 0.5f;
        pw = 0.5f + p.pwm * 0.42f * (0.5f + 0.5f * lfoV); // LFO breathes the width

        const float baseCut = 60.0f * std::pow(200.0f, p.cutoff); // 60 Hz..12 kHz
        const float keyOct = ((float) note - 60.0f) / 12.0f * 0.4f;
        float fc = baseCut * std::exp2(keyOct);
        if (fc < 20.0f) fc = 20.0f;
        const float fcMax = 0.45f * (float) sr;
        if (fc > fcMax) fc = fcMax;
        g = std::tan(3.14159265f * fc / (float) sr);
        k = p.res * 3.6f;
    }

    float render(const PadParams& p) {
        phase += dt;  if (phase >= 1.0f) phase -= 1.0f;
        subPhase += subDt;  if (subPhase >= 1.0f) subPhase -= 1.0f;

        float saw = 2.0f * phase - 1.0f - polyblep(phase, dt);

        float pulse = phase < pw ? 1.0f : -1.0f;
        pulse += polyblep(phase, dt);
        float t2 = phase + (1.0f - pw);
        if (t2 >= 1.0f) t2 -= 1.0f;
        pulse -= polyblep(t2, dt);

        float sub = subPhase < 0.5f ? 1.0f : -1.0f;
        sub += polyblep(subPhase, subDt);
        float t3 = subPhase + 0.5f;
        if (t3 >= 1.0f) t3 -= 1.0f;
        sub -= polyblep(t3, subDt);

        const float mix = saw * 0.45f + pulse * 0.35f + sub * p.subLevel * 0.5f;
        float y = ladder.process(mix, g, k) * (1.0f + k * 0.4f);

        const float e = env.next() + tune::kDenormGuard;
        const float velGain = tune::kBassVelFloor
                            + (1.0f - tune::kBassVelFloor) * velocity;
        return y * e * velGain;
    }

private:
    double sr = 48000.0;
    AmbientADSR env;
    LadderFilter ladder;
    float phase = 0.0f, subPhase = 0.0f;
    float dt = 0.0f, subDt = 0.0f, pw = 0.5f;
    float g = 0.1f, k = 0.0f;
    int note = 60;
    float velocity = 1.0f;
    uint32_t age = 0;
};

} // namespace maru
