#include "AcidPart.h"
#include "../dsp/OscUtils.h"
#include "../../Tuning.h"
#include <cmath>

namespace maru {

// ===== tb303 voice constants (TUNE BY EAR — ported verbatim) =====
namespace {
constexpr float kCutoffMinHz = 80.0f;
constexpr float kCutoffMaxHz = 4200.0f;
constexpr float kEnvModOctaves = 4.3f;
constexpr float kResMaxK = 4.05f;
constexpr float kFilterDrive = 1.15f;
constexpr float kEnvDecayMin = 0.2f, kEnvDecayMax = 2.5f;
constexpr float kEnvAttack = 0.001f;
constexpr float kVcaAttack = 0.002f;
constexpr float kVcaDecay = 2.6f;      // fixed on the 303
constexpr float kVcaRelease = 0.011f;
constexpr float kAccentDecay = 0.13f;  // fixed fast sweep, DECAY knob ignored
constexpr float kAccentOctaves = 2.4f;
constexpr float kAccentVcaGain = 0.9f;
constexpr float kAccentSmoothMax = 0.10f;
constexpr float kSlideTau = 0.055f;    // ~55 ms RC on the pitch CV
constexpr float kGateFrac = 0.55f;
constexpr float kSquareLpHz = 5500.0f;
constexpr float kOutLevel = 0.5f;

inline float coefFor(float tau, float sr) {
    return 1.0f - std::exp(-1.0f / ((tau < 1.0e-5f ? 1.0e-5f : tau) * sr));
}
} // namespace

void AcidPart::prepare(double sampleRate, int) {
    sr = sampleRate;
    ladder.reset();
    phase = 0.0f;
    megEnv = accEnv = accSm = accOn = 0.0f;
    vcaEnv = gateSm = 0.0f;
    megAttPhase = false;
    sqLpState = dcState = 0.0f;
    gate = false;
    controlCountdown = 0;
    stepClock.reset();
    seqWasOn = false;
    seqCurNote = -1;
    seqRng = 0x51f15eedu;
    heldCount = 0;
    glideCoef = coefFor(kSlideTau, (float) sr);
}

void AcidPart::trigger(int note, bool accent) {
    noteTarget = note;
    noteCur = (float) note;
    gate = true;
    megEnv = 0.0f;
    megAttPhase = true;
    vcaEnv = 0.0f;
    if (accent) { accEnv = 1.0f; accOn = params.accent; }
    else accOn = 0.0f;
}

void AcidPart::slideTo(int note, bool accent) {
    noteTarget = note; // gate held through, envelopes NOT retriggered
    gate = true;
    if (accent) { accEnv = 1.0f; accOn = params.accent; }
}

void AcidPart::noteOn(int note, float velocity) {
    if (seq.on) return;
    const bool accent = velocity >= 0.75f;
    velCur = velocity;
    const bool wasHeld = heldCount > 0;
    int w = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[i] != note) held[w++] = held[i];
    heldCount = w;
    if (heldCount < 16) held[heldCount++] = note;
    if (wasHeld && gate) slideTo(note, accent);
    else trigger(note, accent);
}

void AcidPart::noteOff(int note) {
    if (seq.on) return;
    int ix = -1;
    for (int i = 0; i < heldCount; ++i)
        if (held[i] == note) ix = i;
    if (ix < 0) return;
    const bool wasTop = ix == heldCount - 1;
    for (int i = ix; i < heldCount - 1; ++i) held[i] = held[i + 1];
    --heldCount;
    if (heldCount == 0) gateOff();
    else if (wasTop) slideTo(held[heldCount - 1], false);
}

void AcidPart::allNotesOff() {
    heldCount = 0;
    gateOff();
    seqCurNote = -1;
}

int AcidPart::mapDirection(long long stepNum) const {
    const int len = seq.length < 1 ? 1 : (seq.length > 16 ? 16 : seq.length);
    const auto wrap = [](long long v, int m) {
        return (int) (((v % m) + m) % m);
    };
    switch (seq.dir) {
        case 1:  return len - 1 - wrap(stepNum, len);
        case 2: {
            if (len == 1) return 0;
            const int period = 2 * len - 2;
            const int m = wrap(stepNum, period);
            return m < len ? m : period - m;
        }
        default: return wrap(stepNum, len);
    }
}

void AcidPart::fireStep(long long stepNum, double stepBeats, double swingOff) {
    const int len = seq.length < 1 ? 1 : (seq.length > 16 ? 16 : seq.length);
    int patStep, nextPatStep;
    if (seq.dir == 3) {
        patStep = seqRandStep % len;
        uint32_t x = seqRng;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        seqRng = x;
        seqRandStep = (int) (x % (uint32_t) len);
        nextPatStep = seqRandStep;
    } else {
        patStep = mapDirection(stepNum);
        nextPatStep = mapDirection(stepNum + 1);
    }

    const AcidStep& s = pattern.steps[patStep];
    const AcidStep& next = pattern.steps[nextPatStep];

    if (s.gate) {
        const int note = s.note + 12 * (s.oct + params.octave);
        velCur = 1.0f;
        if (s.slide && seqCurNote >= 0) slideTo(note, s.accent);
        else trigger(note, s.accent);
        seqCurNote = note;
        const double stepStart = (double) stepNum * stepBeats
                               + ((stepNum & 1) ? swingOff : 0.0);
        seqGateOffBeat = stepStart + stepBeats * kGateFrac;
        seqHoldForSlide = next.gate && next.slide;
    } else {
        if (seqCurNote >= 0) {
            gateOff();
            seqCurNote = -1;
        }
    }
}

void AcidPart::updateControl() {
    const float srf = (float) sr;
    const AcidParams& p = params;

    baseCut = kCutoffMinHz * std::pow(kCutoffMaxHz / kCutoffMinHz, p.cutoff);
    kRes = p.res * kResMaxK;
    envOctMax = p.envMod * kEnvModOctaves;

    // ACCENT_DECAY_CLAMP: accented notes ignore the DECAY knob entirely
    const float megTau = accOn > 0.0f
        ? kEnvDecayMin
        : kEnvDecayMin * std::pow(kEnvDecayMax / kEnvDecayMin, p.decay);
    megDecCoef = std::exp(-1.0f / (megTau * srf));
    megAttCoef = coefFor(kEnvAttack, srf);
    // the accent "wow": smoothing tau rises with resonance -> accents bloom
    accSmCoef = coefFor(0.004f + p.res * kAccentSmoothMax, srf);
    accDecCoef = std::exp(-1.0f / (kAccentDecay * srf));

    vcaAttCoef = coefFor(kVcaAttack, srf);
    vcaDecCoef = std::exp(-1.0f / (kVcaDecay * srf));
    vcaRelCoef = coefFor(kVcaRelease, srf);

    sqLpCoef = 1.0f - std::exp(-2.0f * 3.14159265f * kSquareLpHz / srf);
    dcCoef = 1.0f - std::exp(-2.0f * 3.14159265f * 20.0f / srf);

    ampGain = (tune::kBassVelFloor + (1.0f - tune::kBassVelFloor) * velCur)
            * p.level * kOutLevel;
}

void AcidPart::render(float* L, float* R, int numSamples, const ClockState& clock) {
    const bool square = params.wave == 1;
    const float srf = (float) sr;

    const double stepBeats = kSeqRateBeats[seq.rate < 0 ? 0 : (seq.rate > 6 ? 6 : seq.rate)];
    const double swingOff = seq.swing * stepBeats * tune::kSeqSwingMax;

    for (int i = 0; i < numSamples; ++i) {
        if (controlCountdown-- <= 0) {
            updateControl();
            controlCountdown = tune::kControlInterval - 1;
        }

        if (seq.on) {
            if (!seqWasOn) {
                seqWasOn = true;
                heldCount = 0;
                gateOff();
                seqCurNote = -1;
                stepClock.reset();
            }
            const double pos = clock.beatAt(i);
            const long long n = stepClock.tick(pos, stepBeats, swingOff);
            if (n != -1) {
                fireStep(n, stepBeats, swingOff);
            } else if (seqCurNote >= 0 && !seqHoldForSlide && pos >= seqGateOffBeat) {
                gateOff();
                seqCurNote = -1;
            }
        } else if (seqWasOn) {
            seqWasOn = false;
            gateOff();
            seqCurNote = -1;
        }

        // -- pitch (slide is an RC on the CV: exponential in semitones) --
        noteCur += ((float) noteTarget - noteCur) * glideCoef;
        const float freq = midiToHz(noteCur);
        const float dt = freq / srf;

        // -- oscillator --
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        float osc;
        if (!square) {
            osc = 2.0f * phase - 1.0f - polyblep(phase, dt);
        } else {
            float sq = phase < 0.5f ? 1.0f : -1.0f;
            sq += polyblep(phase, dt);
            float t2 = phase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
            sq -= polyblep(t2, dt);
            sqLpState += (sq - sqLpState) * sqLpCoef; // 303 shaped-square rounding
            osc = sqLpState * 0.9f;
        }

        // -- envelopes --
        if (megAttPhase) {
            megEnv += (1.0f - megEnv) * megAttCoef;
            if (megEnv > 0.995f) { megEnv = 1.0f; megAttPhase = false; }
        } else {
            megEnv *= megDecCoef;
        }
        megEnv += tune::kDenormGuard;
        accEnv *= accDecCoef;
        accSm += (accEnv * accOn - accSm) * accSmCoef;

        if (gate) {
            gateSm += (1.0f - gateSm) * vcaAttCoef;
            vcaEnv += (1.0f - vcaEnv) * vcaAttCoef;
        } else {
            gateSm += (0.0f - gateSm) * vcaRelCoef;
        }
        vcaEnv *= vcaDecCoef;
        vcaEnv += tune::kDenormGuard;

        // -- filter cutoff (exponential CV summing) --
        float fc = baseCut * std::exp2(envOctMax * megEnv + kAccentOctaves * accSm);
        if (fc > 12000.0f) fc = 12000.0f;
        if (fc < 20.0f) fc = 20.0f;

        // -- ladder + passband-loss makeup --
        const float g = std::tan(3.14159265f * fc / srf);
        float sig = ladder.process(osc * kFilterDrive, g, kRes) * (1.0f + kRes * 0.45f);

        // -- VCA (+ accent boost) --
        const float amp = vcaEnv * gateSm * (1.0f + kAccentVcaGain * accOn * accEnv);
        sig *= amp * ampGain;

        // -- DC block + gentle output clip --
        dcState += (sig - dcState) * dcCoef;
        sig -= dcState;
        sig = std::tanh(sig * 1.25f);

        L[i] = sig;
        R[i] = sig;
    }
}

} // namespace maru
