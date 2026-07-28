#include "BassPart.h"
#include "../dsp/OscUtils.h"
#include "../../Tuning.h"
#include <cmath>

namespace maru {

// ===== avalon voice constants (TUNE BY EAR — ported verbatim) =====
namespace {
constexpr float kCutoffMinHz = 80.0f;
constexpr float kCutoffMaxHz = 4200.0f;
constexpr float kEnvModOctaves = 4.3f;
constexpr float kResMaxK = 4.05f;
constexpr float kFilterDrive = 1.15f;
constexpr float kEnvDecayMin = 0.2f, kEnvDecayMax = 2.0f;
constexpr float kEnvAttack = 0.001f;
constexpr float kVcaAttack = 0.002f;
constexpr float kVcaDecayMin = 0.006f, kVcaDecayMax = 4.0f;
constexpr float kVcaRelease = 0.011f;
constexpr float kAccentOctaves = 2.4f;
constexpr float kAccentVcaGain = 0.9f;
constexpr float kAccentSmoothMax = 0.10f;
constexpr float kAccentDecay = 0.13f;
constexpr float kModAttMin = 0.0009f, kModAttMax = 6.0f;
constexpr float kModDecMin = 0.0017f, kModDecMax = 10.0f;
constexpr float kVcfDepthOctaves = 3.0f;
constexpr float kTrackPivotNote = 39.0f; // D#2 = 77.78 Hz
constexpr float kTrackLowWeight = 1.25f, kTrackHighWeight = 0.75f;
constexpr float kSquareLpHz = 5500.0f;
constexpr float kHp303Hz = 70.0f, kHpFullHz = 18.0f;
constexpr float kOutLevel = 0.45f;

inline float coefFor(float tau, float sr) {
    return 1.0f - std::exp(-1.0f / ((tau < 1.0e-5f ? 1.0e-5f : tau) * sr));
}
} // namespace

void BassPart::prepare(double sampleRate, int) {
    sr = sampleRate;
    ladder.reset();
    phase = subPhase = 0.0f;
    megEnv = accEnv = accSm = accOn = 0.0f;
    vcaEnv = gateSm = modEnv = 0.0f;
    megAttPhase = modAttPhase = false;
    sqLpState = subLpState = dcState = 0.0f;
    gate = false;
    accented = false;
    stepFiltOct = 0.0f;
    controlCountdown = 0;
    stepClock.reset();
    seqWasOn = false;
    seqCurNote = -1;
    seqRng = 0x9e3779b9u;
    heldCount = 0;
    glideCoefStep = coefFor(kSlideTimes[1], (float) sr);
    glideCoef = glideCoefStep;
}

// --- voice control (avalon trigger/slideTo semantics) ---

void BassPart::trigger(int note, bool accent) {
    noteTarget = note;
    noteCur = (float) note; // hard set: no glide on plain trigger
    gate = true;
    megEnv = 0.0f;
    megAttPhase = true;
    vcaEnv = 0.0f;
    accented = accent;
    if (accent) { accEnv = 1.0f; accOn = params.accent; }
    else accOn = 0.0f;
    modAttPhase = true; // mod env retriggers with the note
}

void BassPart::slideTo(int note, bool accent) {
    noteTarget = note;  // envelopes keep running, pitch glides
    gate = true;
    accented = accent;
    if (accent) { accEnv = 1.0f; accOn = params.accent; }
}

// --- live MIDI (seq off): held-note stack with legato slides ---

void BassPart::noteOn(int note, float velocity) {
    if (seq.on) return;
    const bool accent = velocity >= 0.75f;
    velCur = velocity;
    const bool wasHeld = heldCount > 0;
    // remove if already present, then push on top
    int w = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[i] != note) held[w++] = held[i];
    heldCount = w;
    if (heldCount < 16) held[heldCount++] = note;
    glideCoef = glideCoefLive;
    if (wasHeld && gate) slideTo(note, accent);
    else trigger(note, accent);
}

void BassPart::noteOff(int note) {
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

void BassPart::allNotesOff() {
    heldCount = 0;
    gateOff();
    seqCurNote = -1;
}

// --- sequencer (Blacksite direction/step semantics + avalon step fields) ---

int BassPart::mapDirection(long long stepNum) const {
    const int len = seq.length < 1 ? 1 : (seq.length > 16 ? 16 : seq.length);
    const auto wrap = [](long long v, int m) {
        return (int) (((v % m) + m) % m);
    };
    switch (seq.dir) {
        case 1:  return len - 1 - wrap(stepNum, len);            // REV
        case 2: {                                                // PENDULUM
            if (len == 1) return 0;
            const int period = 2 * len - 2;
            const int m = wrap(stepNum, period);
            return m < len ? m : period - m;
        }
        default: return wrap(stepNum, len);                      // FWD
    }
}

void BassPart::fireStep(long long stepNum, double stepBeats, double swingOff) {
    const int len = seq.length < 1 ? 1 : (seq.length > 16 ? 16 : seq.length);
    int patStep, nextPatStep;
    if (seq.dir == 3) { // RANDOM: use the pre-drawn step, draw the next
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

    const BassStep& s = pattern.steps[patStep];
    const BassStep& next = pattern.steps[nextPatStep];

    if (s.gate) {
        const int note = s.note + 12 * s.oct;
        velCur = 1.0f; // accent is the 303 accent circuit, not velocity
        const int slideIx = s.slideT < 0 ? 0 : (s.slideT > 3 ? 3 : s.slideT);
        glideCoefStep = coefFor(kSlideTimes[slideIx], (float) sr);
        glideCoef = glideCoefStep;
        stepFiltOct = s.cvOct;
        if (s.slide && seqCurNote >= 0) slideTo(note, s.accent);
        else trigger(note, s.accent);
        seqCurNote = note;
        const double stepStart = (double) stepNum * stepBeats
                               + ((stepNum & 1) ? swingOff : 0.0);
        const int gateIx = s.gateLen < 0 ? 0 : (s.gateLen > 3 ? 3 : s.gateLen);
        seqGateOffBeat = stepStart + stepBeats * kGateFracs[gateIx];
        seqHoldForSlide = next.gate && next.slide; // tie into a sliding step
    } else {
        if (seqCurNote >= 0) {
            gateOff();
            seqCurNote = -1;
        }
    }
}

// --- control-rate coefficients (every kControlInterval samples) ---

void BassPart::updateControl() {
    const float srf = (float) sr;
    const BassParams& p = params;

    baseCut = kCutoffMinHz * std::pow(kCutoffMaxHz / kCutoffMinHz, p.cutoff);
    kRes = p.res * kResMaxK;
    envOctMax = p.envMod * kEnvModOctaves;

    const float megTau = kEnvDecayMin * std::pow(kEnvDecayMax / kEnvDecayMin,
        accented ? p.accDecay : p.envDecay);
    megDecCoef = std::exp(-1.0f / (megTau * srf));
    megAttCoef = coefFor(kEnvAttack, srf);
    accSmCoef = coefFor(0.004f + p.res * kAccentSmoothMax, srf); // resonance "wow"
    accDecCoef = std::exp(-1.0f / (kAccentDecay * srf));

    const float vcaTau = kVcaDecayMin * std::pow(kVcaDecayMax / kVcaDecayMin, p.vcaDecay);
    vcaDecCoef = std::exp(-1.0f / (vcaTau * srf));
    vcaAttCoef = coefFor(kVcaAttack, srf);
    vcaRelCoef = coefFor(kVcaRelease, srf);

    modAttCoef = coefFor(kModAttMin * std::pow(kModAttMax / kModAttMin, p.modAtt), srf);
    modDecCoef = std::exp(-1.0f
        / (kModDecMin * std::pow(kModDecMax / kModDecMin, p.modDec) * srf));

    vcfDepthOct = p.vcfModDepth * kVcfDepthOctaves;
    vcaDepthGain = p.vcaModDepth;

    sqLpCoef = 1.0f - std::exp(-2.0f * 3.14159265f * kSquareLpHz / srf);
    const float hpHz = p.fr == 1 ? kHpFullHz : kHp303Hz;
    dcCoef = 1.0f - std::exp(-2.0f * 3.14159265f * hpHz / srf);

    glideCoefLive = coefFor(p.glide, srf);

    subLvl = p.subLevel * tune::kSubLevelGain;
    ampGain = (tune::kBassVelFloor + (1.0f - tune::kBassVelFloor) * velCur)
            * p.level * kOutLevel;
}

// --- render ---

void BassPart::render(float* L, float* R, int numSamples, const ClockState& clock) {
    const BassParams& p = params;
    const int mainWave = p.wave;
    const int subWave = p.subWave;
    const float subDiv = p.subOct >= 2 ? 4.0f : 2.0f;
    const float srf = (float) sr;

    const double stepBeats = kSeqRateBeats[seq.rate < 0 ? 0 : (seq.rate > 6 ? 6 : seq.rate)];
    const double swingOff = seq.swing * stepBeats * tune::kSeqSwingMax;

    for (int i = 0; i < numSamples; ++i) {
        if (controlCountdown-- <= 0) {
            updateControl();
            controlCountdown = tune::kControlInterval - 1;
        }

        // -- sequencer: sample-counted on the shared musical clock --
        if (seq.on) {
            if (!seqWasOn) {
                seqWasOn = true;
                heldCount = 0; // take the voice over cleanly from live MIDI
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

        // -- pitch (one-pole RC glide in semitone units; envelopes NOT retriggered) --
        noteCur += ((float) noteTarget - noteCur) * glideCoef;
        const float freq = midiToHz(noteCur);
        const float dt = freq / srf;

        // -- main oscillator --
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        float osc = 0.0f;
        if (mainWave == 0) {
            osc = 2.0f * phase - 1.0f - polyblep(phase, dt);
        } else if (mainWave == 2) {
            float sq = phase < 0.5f ? 1.0f : -1.0f;
            sq += polyblep(phase, dt);
            float t2 = phase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
            sq -= polyblep(t2, dt);
            sqLpState += (sq - sqLpState) * sqLpCoef; // 303 wave-shaper rounding
            osc = sqLpState * 0.9f;
        }

        // -- sub oscillator (pre-filter, avalon topology) --
        const float sdt = dt / subDiv;
        subPhase += sdt;
        if (subPhase >= 1.0f) subPhase -= 1.0f;
        float sub = 0.0f;
        if (subLvl > 0.001f) {
            if (subWave == 0) {
                sub = 2.0f * subPhase - 1.0f - polyblep(subPhase, sdt);
            } else if (subWave == 1) {
                sub = 2.0f * std::fabs(2.0f * subPhase - 1.0f) - 1.0f; // triangle
            } else {
                float sq = subPhase < 0.5f ? 1.0f : -1.0f;
                sq += polyblep(subPhase, sdt);
                float t2 = subPhase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
                sq -= polyblep(t2, sdt);
                subLpState += (sq - subLpState) * sqLpCoef;
                sub = subLpState * 0.9f;
            }
        }
        const float mix = osc * 0.8f + sub * subLvl;

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

        if (modAttPhase) {
            modEnv += (1.0f - modEnv) * modAttCoef;
            if (modEnv > 0.995f) { modEnv = 1.0f; modAttPhase = false; }
        } else {
            modEnv *= modDecCoef;
        }
        modEnv += tune::kDenormGuard;

        if (gate) {
            gateSm += (1.0f - gateSm) * vcaAttCoef;
            vcaEnv += (1.0f - vcaEnv) * vcaAttCoef;
        } else {
            gateSm += (0.0f - gateSm) * vcaRelCoef;
        }
        vcaEnv *= vcaDecCoef;
        vcaEnv += tune::kDenormGuard;

        // -- filter cutoff CV sum (log tracking around D#2, mod env offset trick) --
        const float dNote = noteCur - kTrackPivotNote;
        const float trackOct = p.tracking * (dNote / 12.0f)
            * (dNote < 0.0f ? kTrackLowWeight : kTrackHighWeight);
        const float vcfModOct = vcfDepthOct * (modEnv - 1.0f);
        float fc = baseCut * std::exp2(
            envOctMax * megEnv + kAccentOctaves * accSm
            + trackOct + vcfModOct + stepFiltOct);
        if (fc > 12000.0f) fc = 12000.0f;
        if (fc < 20.0f) fc = 20.0f;

        // -- 4-pole ZDF ladder, tanh drive --
        const float g = std::tan(3.14159265f * fc / srf);
        float sig = ladder.process(mix * kFilterDrive, g, kRes) * (1.0f + kRes * 0.45f);

        // -- VCA (+ accent boost, + mod env depth; negative depth = drone) --
        float amp = vcaEnv * gateSm * (1.0f + kAccentVcaGain * accOn * accEnv);
        if (vcaDepthGain >= 0.0f) {
            const float m = 1.0f + vcaDepthGain * (modEnv - 1.0f);
            amp *= m > 0.0f ? m : 0.0f;
        } else {
            amp += -vcaDepthGain * (1.0f - modEnv) * 0.8f;
        }
        sig *= amp * ampGain;

        // -- FR high-pass + gentle output clip --
        dcState += (sig - dcState) * dcCoef;
        sig -= dcState;
        sig = std::tanh(sig * 1.25f);

        L[i] = sig;
        R[i] = sig;
    }
}

} // namespace maru
