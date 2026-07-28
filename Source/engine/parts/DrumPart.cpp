#include "DrumPart.h"
#include "../../Tuning.h"
#include <cmath>

namespace maru {

void DrumPart::prepare(double sampleRate, int) {
    sr = sampleRate;
    defaultKit.prepare(sampleRate);
    for (int i = 0; i < 8; ++i) {
        voice[i].prepare(sampleRate);
        ext[i] = nullptr;
        lpL[i] = lpR[i] = 0.0f;
    }
    stepClock.reset();
    seqWasOn = false;
    seqRng = 0x00d80909u;
    controlCountdown = 0;
}

void DrumPart::setSamples(const SampleBuffer* const bufs[8]) {
    for (int i = 0; i < 8; ++i) ext[i] = bufs[i];
}

const SampleBuffer* DrumPart::bufFor(int pad) const {
    return ext[pad] != nullptr ? ext[pad] : defaultKit.pad(pad);
}

void DrumPart::firePad(int pad, float velocity) {
    if (pad < 0 || pad >= 8) return;
    const DrumPadParams& pp = params.pads[pad];

    // choke: silence every other pad sharing this pad's choke group
    if (pp.choke > 0)
        for (int o = 0; o < 8; ++o)
            if (o != pad && params.pads[o].choke == pp.choke)
                voice[o].choke();

    const SampleBuffer* buf = bufFor(pad);
    if (buf == nullptr) return;
    const double ratio = std::exp2((double) pp.tune / 12.0) * buf->sourceRate / sr;
    // decay 0..1 log-mapped 10 ms .. 4 s; >= 0.98 lets the sample ring out
    const float decaySec = pp.decay >= 0.98f
        ? -1.0f
        : 0.010f * std::pow(400.0f, pp.decay);
    const float gain = (tune::kBassVelFloor + (1.0f - tune::kBassVelFloor) * velocity)
                     * pp.level;
    voice[pad].trigger(buf, ratio, gain, decaySec);
}

void DrumPart::trigger(int pad, float velocity) { firePad(pad, velocity); }

void DrumPart::allNotesOff() {
    for (auto& v : voice) v.choke();
}

int DrumPart::mapDirection(long long stepNum) const {
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

void DrumPart::fireStep(long long stepNum) {
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

    const float accVel = tune::kSeqPlainVel
        + (tune::kSeqAccentVel - tune::kSeqPlainVel)
          * (params.accentScale > 1.0f ? 1.0f : params.accentScale);
    for (int lane = 0; lane < 8; ++lane) {
        const DrumCell& c = grids[activeBank].cells[lane][patStep];
        if (c.on)
            firePad(lane, c.accent ? accVel : tune::kSeqPlainVel);
    }
}

void DrumPart::render(float* L, float* R, int numSamples, const ClockState& clock) {
    const double stepBeats = kSeqRateBeats[seq.rate < 0 ? 0 : (seq.rate > 6 ? 6 : seq.rate)];
    const double swingOff = seq.swing * stepBeats * tune::kSeqSwingMax;

    for (int i = 0; i < numSamples; ++i) {
        if (controlCountdown-- <= 0) {
            controlCountdown = tune::kControlInterval - 1;
            for (int p = 0; p < 8; ++p) {
                const DrumPadParams& pp = params.pads[p];
                // pad LP: 0..1 -> 200 Hz..18 kHz exponential, 1.0 ~ open
                const float hz = 200.0f * std::pow(90.0f, pp.cutoff);
                lpCoef[p] = 1.0f - std::exp(-6.2831853f * hz / (float) sr);
                const float ang = (pp.pan + 1.0f) * 0.25f * 3.14159265f;
                panL[p] = std::cos(ang) * 1.41421356f;
                panR[p] = std::sin(ang) * 1.41421356f;
            }
        }

        if (seq.on) {
            if (!seqWasOn) {
                seqWasOn = true;
                stepClock.reset();
            }
            const double pos = clock.beatAt(i);
            const long long bar = (long long) std::floor(pos * 0.25);
            if (bar != lastBar) {
                lastBar = bar;
                activeBank = seq.bank < 0 ? 0 : (seq.bank >= kNumBanks ? kNumBanks - 1 : seq.bank);
            }
            const long long n = stepClock.tick(pos, stepBeats, swingOff);
            if (n != -1)
                fireStep(n);
        } else if (seqWasOn) {
            seqWasOn = false;
        }

        float outL = 0.0f, outR = 0.0f;
        for (int p = 0; p < 8; ++p) {
            if (!voice[p].active()) continue;
            float l, r;
            voice[p].next(bufFor(p), l, r);
            lpL[p] += (l - lpL[p]) * lpCoef[p];
            lpR[p] += (r - lpR[p]) * lpCoef[p];
            outL += lpL[p] * panL[p];
            outR += lpR[p] * panR[p];
        }
        L[i] = outL;
        R[i] = outR;
    }
}

} // namespace maru
