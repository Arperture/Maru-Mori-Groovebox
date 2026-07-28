#include "PadPart.h"
#include <cmath>

namespace maru {

namespace {
// norm 0..1 -> seconds, log-mapped (pads reach very long segments)
inline float segSeconds(float v) {
    return 0.005f * std::pow(2400.0f, v); // 5 ms .. 12 s
}
} // namespace

void PadPart::prepare(double sampleRate, int) {
    sr = sampleRate;
    for (auto& v : voices) {
        v.prepare(sampleRate);
        v.kill();
    }
    chorus.prepare(sampleRate);
    ageCounter = 0;
    stepClock.reset();
    seqWasOn = false;
    curChord = -1;
    seqRng = 0x9adface1u;
    lfoPhase = lfoRamp = 0.0f;
    controlCountdown = 0;
}

// abio allocator: idle -> quietest-releasing -> oldest
PadVoice* PadPart::allocateVoice() {
    for (auto& v : voices)
        if (v.isIdle()) return &v;
    PadVoice* best = nullptr;
    for (auto& v : voices)
        if (v.isReleasing() && (best == nullptr || v.envLevel() < best->envLevel()))
            best = &v;
    if (best != nullptr) return best;
    for (auto& v : voices)
        if (best == nullptr || v.voiceAge() < best->voiceAge())
            best = &v;
    return best;
}

void PadPart::noteOnInternal(int note, float vel) {
    // retrigger a held instance of the same note instead of stacking
    for (auto& v : voices) {
        if (!v.isIdle() && !v.isReleasing() && v.currentNote() == note) {
            v.noteOn(note, vel, ++ageCounter);
            return;
        }
    }
    allocateVoice()->noteOn(note, vel, ++ageCounter);
}

void PadPart::noteOffInternal(int note) {
    for (auto& v : voices)
        if (!v.isIdle() && !v.isReleasing() && v.currentNote() == note)
            v.noteOff();
}

void PadPart::noteOn(int note, float velocity) {
    if (seq.on) return;
    noteOnInternal(note, velocity);
}

void PadPart::noteOff(int note) {
    if (seq.on) return;
    noteOffInternal(note);
}

void PadPart::allNotesOff() {
    for (auto& v : voices) v.noteOff();
    curChord = -1;
}

void PadPart::triggerChord(int chordIx) {
    const int ix = chordIx < 0 ? 0 : (chordIx > 3 ? 3 : chordIx);
    const PadChord& c = patterns[activeBank].chords[ix];
    for (int i = 0; i < c.count && i < 4; ++i)
        noteOnInternal(c.notes[i], 0.8f);
    curChord = ix;
}

void PadPart::releaseChord() {
    if (curChord < 0) return;
    const PadChord& c = patterns[activeBank].chords[curChord];
    for (int i = 0; i < c.count && i < 4; ++i)
        noteOffInternal(c.notes[i]);
    curChord = -1;
}

int PadPart::mapDirection(long long stepNum) const {
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

void PadPart::fireStep(long long stepNum) {
    const int len = seq.length < 1 ? 1 : (seq.length > 16 ? 16 : seq.length);
    int patStep;
    if (seq.dir == 3) {
        patStep = seqRandStep % len;
        uint32_t x = seqRng;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        seqRng = x;
        seqRandStep = (int) (x % (uint32_t) len);
    } else {
        patStep = mapDirection(stepNum);
    }

    const PadStep& s = patterns[activeBank].steps[patStep];
    if (s.tie)
        return;                 // hold whatever is sounding
    if (s.gate) {
        releaseChord();         // change chords with a crossfading release
        triggerChord(s.chord);
    } else {
        releaseChord();         // rest
    }
}

void PadPart::render(float* L, float* R, int numSamples, const ClockState& clock) {
    const PadParams& p = params;
    const double stepBeats = kSeqRateBeats[seq.rate < 0 ? 0 : (seq.rate > 6 ? 6 : seq.rate)];
    const double swingOff = seq.swing * stepBeats * tune::kSeqSwingMax;

    const float att = segSeconds(p.att);
    const float dec = segSeconds(p.dec);
    const float rel = segSeconds(p.rel);

    for (int i = 0; i < numSamples; ++i) {
        if (controlCountdown-- <= 0) {
            controlCountdown = tune::kControlInterval - 1;

            // part LFO with delay ramp: silence resets it, notes fade it in
            bool anySounding = false;
            for (auto& v : voices)
                if (!v.isIdle()) { anySounding = true; break; }
            const float hz = 0.1f * std::pow(80.0f, p.lfoRate); // 0.1..8 Hz
            lfoPhase += hz * (float) tune::kControlInterval / (float) sr;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            if (!anySounding) {
                lfoRamp = 0.0f;
            } else {
                const float rampSec = 0.3f + p.lfoDelay * 3.0f;
                lfoRamp += (1.0f - lfoRamp)
                         * (1.0f - std::exp(-(float) tune::kControlInterval
                                            / (rampSec * (float) sr)));
            }
            lfoV = std::sin(kTwoPiF * lfoPhase) * p.lfoDepth * lfoRamp;

            for (auto& v : voices) {
                if (v.isIdle()) continue;
                v.setTimes(att, dec, p.sus, rel);
                v.updateControl(p, lfoV);
            }
        }

        if (seq.on) {
            if (!seqWasOn) {
                seqWasOn = true;
                stepClock.reset();
                curChord = -1;
            }
            const double pos = clock.beatAt(i);
            // release with the OLD bank's chord table before switching banks
            const long long bar = (long long) std::floor(pos * 0.25);
            if (bar != lastBar) {
                lastBar = bar;
                const int req = seq.bank < 0 ? 0
                              : (seq.bank >= kNumBanks ? kNumBanks - 1 : seq.bank);
                if (req != activeBank) {
                    releaseChord();
                    activeBank = req;
                }
            }
            const long long n = stepClock.tick(pos, stepBeats, swingOff);
            if (n != -1)
                fireStep(n);
        } else if (seqWasOn) {
            seqWasOn = false;
            releaseChord();
        }

        float m = 0.0f;
        for (auto& v : voices)
            if (!v.isIdle())
                m += v.render(p);
        m *= p.level * 0.35f; // 4-note chords need headroom

        float l = m, r = m;
        chorus.process(l, r, p.chorusMode, 1.0f);
        L[i] = l;
        R[i] = r;
    }
}

} // namespace maru
