#include "BassPart.h"
#include "../dsp/OscUtils.h"
#include "../../Tuning.h"
#include <cmath>

namespace maru {

void BassPart::prepare(double sampleRate, int) {
    sr = sampleRate;
    ladder.reset();
    oscPhase = subPhase = 0.0f;
    vcaEnv = modEnv = gateSm = 0.0f;
    gate = false;
    controlCountdown = 0;
    stepClock.reset();
}

void BassPart::noteOn(int note, float velocity) {
    noteTarget = (float) note;
    if (!gate)
        noteCur = noteTarget; // no glide from silence
    gate = true;
    velCur = velocity;
    vcaEnv = 0.0f;
    modEnv = 0.0f;
    modEnvRising = true;
}

void BassPart::noteOff(int note) {
    if ((int) (noteTarget + 0.5f) == note)
        gate = false;
}

void BassPart::allNotesOff() { gate = false; }

void BassPart::updateControl() {
    const float srf = (float) sr;
    const BassParams& p = params;

    glideCoef  = 1.0f - std::exp(-1.0f / (p.glide < 0.001f ? 0.001f : p.glide) / srf);
    gateCoef   = 1.0f - std::exp(-1.0f / (0.002f * srf));
    vcaAttCoef = 1.0f - std::exp(-1.0f / (tune::kBassVcaAttack * srf));

    // decays log-mapped 0.05 .. 4 s
    const float decS = 0.05f * std::pow(80.0f, p.decayNorm);
    vcaDecCoef = std::exp(-1.0f / (decS * srf));

    const float mAttS = 0.001f * std::pow(6000.0f, p.modAtt); // 1 ms .. 6 s
    const float mDecS = 0.002f * std::pow(5000.0f, p.modDec); // 2 ms .. 10 s
    modAttCoef = 1.0f - std::exp(-1.0f / (mAttS * srf));
    modDecCoef = std::exp(-1.0f / (mDecS * srf));

    const float hz = midiToHz(noteCur);
    dt = hz / srf;
    const float subDiv = p.subOct >= 2 ? 4.0f : 2.0f;
    subDt = dt / subDiv;

    // exponential cutoff CV: base cutoff swept by the mod env (bipolar via
    // the avalon inverted-offset trick: depth*(env-1) sweeps back to base)
    const float baseCut = tune::kBassCutoffMinHz
        * std::pow(tune::kBassCutoffMaxHz / tune::kBassCutoffMinHz, p.cutoff);
    const float envOct = p.envMod * tune::kBassEnvOctaves * (modEnv - 1.0f) * -1.0f;
    float fc = baseCut * std::exp2(envOct * (p.envMod >= 0.0f ? 1.0f : -1.0f));
    if (fc < 20.0f) fc = 20.0f;
    const float fcMax = 0.45f * srf;
    if (fc > fcMax) fc = fcMax;
    g = std::tan(3.14159265f * fc / srf);

    k = p.res * tune::kBassResMaxK;
    resMakeup = 1.0f + k * 0.4f;

    ampGain = (tune::kBassVelFloor + (1.0f - tune::kBassVelFloor) * velCur)
            * p.level * tune::kBassOutTrim;
}

void BassPart::render(float* L, float* R, int numSamples, const ClockState&) {
    const BassParams& p = params;
    const float sawOn = p.wave == 0 ? 1.0f : 0.0f;
    const float sqOn  = p.wave == 2 ? 1.0f : 0.0f;
    const float subLvl = p.subLevel * tune::kSubLevelGain;

    for (int i = 0; i < numSamples; ++i) {
        if (controlCountdown-- <= 0) {
            updateControl();
            controlCountdown = tune::kControlInterval - 1;
        }

        noteCur += (noteTarget - noteCur) * glideCoef;
        gateSm += ((gate ? 1.0f : 0.0f) - gateSm) * gateCoef;

        // envelopes
        if (gate) vcaEnv += (1.0f - vcaEnv) * vcaAttCoef;
        else      vcaEnv *= vcaDecCoef;
        vcaEnv += tune::kDenormGuard;

        if (modEnvRising) {
            modEnv += (1.02f - modEnv) * modAttCoef;
            if (modEnv >= 1.0f) { modEnv = 1.0f; modEnvRising = false; }
        } else {
            modEnv *= modDecCoef;
        }
        modEnv += tune::kDenormGuard;

        // oscillators (pre-filter mix, avalon topology)
        oscPhase += dt; if (oscPhase >= 1.0f) oscPhase -= 1.0f;
        subPhase += subDt; if (subPhase >= 1.0f) subPhase -= 1.0f;

        float main = 0.0f;
        if (sawOn > 0.0f)
            main = 2.0f * oscPhase - 1.0f - polyblep(oscPhase, dt);
        else if (sqOn > 0.0f) {
            main = oscPhase < 0.5f ? 1.0f : -1.0f;
            main += polyblep(oscPhase, dt);
            float t2 = oscPhase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
            main -= polyblep(t2, dt);
        }

        float sub;
        switch (p.subWave) {
            case 1:  sub = 4.0f * std::fabs(subPhase - 0.5f) - 1.0f; break; // tri
            case 2: { // square (BLEP)
                sub = subPhase < 0.5f ? 1.0f : -1.0f;
                sub += polyblep(subPhase, subDt);
                float t2 = subPhase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
                sub -= polyblep(t2, subDt);
                break;
            }
            default: sub = std::sin(kTwoPiF * subPhase); break; // sine
        }

        const float x = (main * 0.8f + sub * subLvl) * tune::kBassDrive;
        float y = ladder.process(x, g, k) * resMakeup;

        y *= vcaEnv * gateSm * ampGain;
        L[i] = y;
        R[i] = y;
    }
}

} // namespace maru
