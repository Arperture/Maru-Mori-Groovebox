#pragma once
#include <cmath>
#include "../../Tuning.h"

// Ported from abiogenesis/Source/engine/AmbientADSR.h (fleet shared).

namespace maru {

// RC-exponential ADSR with segment times up to 30 s. Attack chases an
// overshoot target so it reaches full level in finite time; retriggering
// restarts the attack from the current level (click-free voice reuse).
class AmbientADSR {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate) { sr = sampleRate; }

    void setTimes(float attackS, float decayS, float sustainLvl, float releaseS) {
        aCoef = coefFor(attackS);
        dCoef = coefFor(decayS);
        rCoef = coefFor(releaseS);
        sustain = sustainLvl;
    }

    void noteOn()  { stage = Stage::Attack; }
    void noteOff() {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }
    void kill()    { stage = Stage::Idle; level = 0.0f; }

    float next() {
        switch (stage) {
            case Stage::Attack:
                level += aCoef * (tune::kAttackOvershoot - level);
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level += dCoef * (sustain - level);
                if (level - sustain < 1.0e-3f) stage = Stage::Sustain;
                break;
            case Stage::Sustain:
                level = sustain;
                break;
            case Stage::Release:
                level += rCoef * (0.0f - level);
                if (level < tune::kEnvIdleFloor) { level = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                break;
        }
        return level;
    }

    bool  isIdle()      const { return stage == Stage::Idle; }
    bool  isReleasing() const { return stage == Stage::Release; }
    float value()       const { return level; }

private:
    float coefFor(float seconds) const {
        float s = seconds < 0.001f ? 0.001f : seconds;
        return 1.0f - std::exp(-1.0f / (s * (float) sr));
    }

    double sr = 48000.0;
    Stage  stage = Stage::Idle;
    float  level = 0.0f;
    float  aCoef = 0.01f, dCoef = 0.001f, rCoef = 0.0005f, sustain = 0.8f;
};

} // namespace maru
