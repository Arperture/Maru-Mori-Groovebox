#include "GrooveEngine.h"
#include "../Tuning.h"
#include <cmath>

namespace maru {

void GrooveEngine::prepare(double sampleRate, int maxBlockSize) {
    sr = sampleRate;
    bass.prepare(sampleRate, maxBlockSize);
    for (int p = 0; p < 4; ++p) {
        partL[p].assign((size_t) maxBlockSize, 0.0f);
        partR[p].assign((size_t) maxBlockSize, 0.0f);
    }
    sendDL.assign((size_t) maxBlockSize, 0.0f);
    sendDR.assign((size_t) maxBlockSize, 0.0f);
    sendRL.assign((size_t) maxBlockSize, 0.0f);
    sendRR.assign((size_t) maxBlockSize, 0.0f);
    musicalPos = 0.0;
    lastSeenPpq = -1.0e18;
    hpL = hpR = 0.0f;
    for (int p = 0; p < 4; ++p) {
        smLevel[p] = params.mix.level[p];
        smPan[p]   = params.mix.pan[p];
    }
}

void GrooveEngine::setPatterns(const GroovePatterns& g) {
    bass.setPattern(g.bass);
}

void GrooveEngine::noteOn(int midiChannel, int note, float velocity) {
    switch (midiChannel) {
        case 2: break; // acid (M3)
        case 3: break; // drums (M4)
        case 4: break; // pad (M5)
        default: bass.noteOn(note, velocity); break;
    }
}

void GrooveEngine::noteOff(int midiChannel, int note) {
    switch (midiChannel) {
        case 2: case 3: case 4: break;
        default: bass.noteOff(note); break;
    }
}

void GrooveEngine::allNotesOff() {
    bass.allNotesOff();
}

void GrooveEngine::process(float* left, float* right, int numSamples,
                           const TransportInfo& transport) {
    // -- musical clock: hard-sync to a freshly-reported host ppq (also handles
    // backward jumps at loop points), then advance sample-counted. Segment
    // calls within one block re-see the same ppq and keep advancing.
    const double bpm = transport.playing && transport.bpm > 1.0
                     ? transport.bpm : params.freeBpm;
    const double beatsPerSample = bpm / 60.0 / sr;
    if (transport.playing && transport.ppq != lastSeenPpq) {
        musicalPos = transport.ppq;
        lastSeenPpq = transport.ppq;
    }

    ClockState clock;
    clock.blockStartBeat = musicalPos;
    clock.beatsPerSample = beatsPerSample;
    clock.bpm = bpm;
    clock.playing = transport.playing;
    musicalPos += numSamples * beatsPerSample;

    // -- parts render into scratch (each OVERWRITES its buffers)
    bass.setParams(params.bass, params.seq[0]);
    bass.render(partL[0].data(), partR[0].data(), numSamples, clock);
    for (int p = 1; p < 4; ++p) {
        for (int i = 0; i < numSamples; ++i) {
            partL[p][i] = 0.0f;
            partR[p][i] = 0.0f;
        }
    }

    // -- mixer: smoothed level + equal-power pan -> dry sum; post-level sends
    const float smoothCoef = 1.0f - std::exp(-1.0f
        / (tune::kMixSmoothTau * (float) sr / (float) tune::kControlInterval));

    for (int i = 0; i < numSamples; ++i) {
        sendDL[i] = sendDR[i] = sendRL[i] = sendRR[i] = 0.0f;
        left[i] = tune::kDenormGuard;
        right[i] = tune::kDenormGuard;
    }

    for (int p = 0; p < 4; ++p) {
        for (int i = 0; i < numSamples; ++i) {
            if ((i % tune::kControlInterval) == 0) {
                smLevel[p] += (params.mix.level[p] - smLevel[p]) * smoothCoef;
                smPan[p]   += (params.mix.pan[p]   - smPan[p])   * smoothCoef;
            }
            const float ang = (smPan[p] + 1.0f) * 0.25f * 3.14159265f;
            const float pl = std::cos(ang), pr = std::sin(ang);
            const float xl = partL[p][i] * smLevel[p] * pl * 1.41421356f;
            const float xr = partR[p][i] * smLevel[p] * pr * 1.41421356f;
            left[i]  += xl;
            right[i] += xr;
            sendDL[i] += xl * params.mix.dlySend[p];
            sendDR[i] += xr * params.mix.dlySend[p];
            sendRL[i] += xl * params.mix.vrbSend[p];
            sendRR[i] += xr * params.mix.vrbSend[p];
        }
    }

    // -- send FX (M1): tanh-guard the summed sends, StereoDelay 100% wet,
    // delay->reverb feed, BloomReverb 100% wet, returns to master.
    // M0: sends are summed but the FX are not yet instantiated.

    // -- master chain: HP -> tanh guard -> gain
    const float hpHz = params.master.hpMode == 1 ? tune::kMasterHpHiHz
                                                 : tune::kMasterHpLoHz;
    const float hpCoef = 1.0f - std::exp(-2.0f * 3.14159265f * hpHz / (float) sr);
    const float gd = tune::kMasterGuardDrive;
    const float gain = params.master.gain;

    for (int i = 0; i < numSamples; ++i) {
        hpL += (left[i]  - hpL) * hpCoef;
        hpR += (right[i] - hpR) * hpCoef;
        float l = left[i]  - hpL;
        float r = right[i] - hpR;
        l = std::tanh(l * gd) / gd;
        r = std::tanh(r * gd) / gd;
        left[i]  = l * gain;
        right[i] = r * gain;
    }
}

} // namespace maru
