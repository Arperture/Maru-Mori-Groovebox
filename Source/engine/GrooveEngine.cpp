#include "GrooveEngine.h"
#include "../Tuning.h"
#include <cmath>

namespace maru {

void GrooveEngine::prepare(double sampleRate, int maxBlockSize) {
    sr = sampleRate;
    bass.prepare(sampleRate, maxBlockSize);
    acid.prepare(sampleRate, maxBlockSize);
    drums.prepare(sampleRate, maxBlockSize);
    pad.prepare(sampleRate, maxBlockSize);
    delay.prepare(sampleRate);
    reverb.prepare(sampleRate);
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
    acid.setPattern(g.acid);
    drums.setGrid(g.drums);
    pad.setPattern(g.pad);
}

void GrooveEngine::noteOn(int midiChannel, int note, float velocity) {
    switch (midiChannel) {
        case 2: acid.noteOn(note, velocity); break;
        case 3: drums.trigger(note - 36, velocity); break; // notes 36..43 -> pads
        case 4: pad.noteOn(note, velocity); break;
        default: bass.noteOn(note, velocity); break;
    }
}

void GrooveEngine::noteOff(int midiChannel, int note) {
    switch (midiChannel) {
        case 2: acid.noteOff(note); break;
        case 3: break; // one-shots: note-off is meaningless
        case 4: pad.noteOff(note); break;
        default: bass.noteOff(note); break;
    }
}

void GrooveEngine::allNotesOff() {
    bass.allNotesOff();
    acid.allNotesOff();
    drums.allNotesOff();
    pad.allNotesOff();
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
    acid.setParams(params.acid, params.seq[1]);
    acid.render(partL[1].data(), partR[1].data(), numSamples, clock);
    drums.setParams(params.drum, params.seq[2]);
    drums.render(partL[2].data(), partR[2].data(), numSamples, clock);
    pad.setParams(params.pad, params.seq[3]);
    pad.render(partL[3].data(), partR[3].data(), numSamples, clock);

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

    // -- send FX: tanh-guard the summed sends, StereoDelay 100% wet,
    // delay->reverb feed (the Valhalla move: echoes bloom into the tail),
    // BloomReverb 100% wet, returns added to the dry sum.
    const float dlyTimeS = params.dly.sync
        ? (float) (kDelayDivBeats[params.dly.div < 0 ? 0
                     : (params.dly.div > 8 ? 8 : params.dly.div)] * 60.0 / bpm)
        : params.dly.timeS;
    const float toVerb = params.dly.toVerb > tune::kDlyToVerbMax
                       ? tune::kDlyToVerbMax : params.dly.toVerb;
    BloomReverb::Params vp;
    vp.decay = params.vrb.decay;
    vp.size = params.vrb.size;
    vp.predelayMs = params.vrb.predelayMs;
    vp.modDepth = params.vrb.modDepth;
    vp.modRate = params.vrb.modRate;
    vp.lowDamp = params.vrb.lowDamp;
    vp.highDamp = params.vrb.highDamp;
    vp.shimmer = params.vrb.shimmer;
    vp.shimInterval = params.vrb.shimInterval;
    vp.freeze = params.vrb.freeze;

    const float sd = tune::kSendDrive;
    for (int i = 0; i < numSamples; ++i) {
        const float dInL = std::tanh(sendDL[i] * sd) / sd;
        const float dInR = std::tanh(sendDR[i] * sd) / sd;
        float dOutL, dOutR;
        delay.process(dInL, dInR, dOutL, dOutR, params.dly.mode,
                      dlyTimeS, params.dly.feedback, params.dly.tone);

        const float rSumL = sendRL[i] + dOutL * toVerb;
        const float rSumR = sendRR[i] + dOutR * toVerb;
        const float rInL = std::tanh(rSumL * sd) / sd;
        const float rInR = std::tanh(rSumR * sd) / sd;
        float vOutL, vOutR;
        reverb.process(rInL, rInR, vOutL, vOutR, vp);

        left[i]  += dOutL * params.dly.ret + vOutL * params.vrb.ret;
        right[i] += dOutR * params.dly.ret + vOutR * params.vrb.ret;
    }

    // -- master chain: HP -> tanh guard -> gain
    const float hpHz = params.master.hpMode == 1 ? tune::kMasterHpHiHz
                                                 : tune::kMasterHpLoHz;
    const float hpCoef = 1.0f - std::exp(-2.0f * 3.14159265f * hpHz / (float) sr);
    const float gain = params.master.gain;

    // HP -> gain -> tanh soft clip. The clip ceiling is exactly 1.0 and it is
    // transparent below ~-12 dBFS (<1% H3 on the sub); this is the mix bus's
    // final safety, not a tone stage.
    for (int i = 0; i < numSamples; ++i) {
        hpL += (left[i]  - hpL) * hpCoef;
        hpR += (right[i] - hpR) * hpCoef;
        left[i]  = std::tanh((left[i]  - hpL) * gain);
        right[i] = std::tanh((right[i] - hpR) * gain);
    }
}

} // namespace maru
