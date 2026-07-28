// MaruRenderTest — offline engine verification, no JUCE, no audio device.
// Usage: MaruRenderTest [outBase] [--case <name>]
// Writes <outBase>-<case>.wav per case for listening. Non-zero exit on failure.
// Harness pattern ported from abiogenesis/tests/RenderTest.cpp (fleet law:
// every case pairs "finite" with an UPPER bound — blowups can be finite).

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include "engine/GrooveEngine.h"

using namespace maru;

namespace {

constexpr double kSR = 48000.0;
constexpr int    kBlock = 512;

std::string gOutBase = "marumori-render";

// ---------------------------------------------------------------- helpers --

void writeWav(const std::string& path, const std::vector<float>& l,
              const std::vector<float>& r) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    const uint32_t n = (uint32_t) l.size();
    const uint32_t dataBytes = n * 2 * 2;
    const uint32_t sr = (uint32_t) kSR;
    const uint32_t byteRate = sr * 4;
    uint8_t hdr[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E',
                        'f','m','t',' ', 16,0,0,0, 1,0, 2,0,
                        0,0,0,0, 0,0,0,0, 4,0, 16,0,
                        'd','a','t','a', 0,0,0,0 };
    const uint32_t riffSize = 36 + dataBytes;
    std::memcpy(hdr + 4, &riffSize, 4);
    std::memcpy(hdr + 24, &sr, 4);
    std::memcpy(hdr + 28, &byteRate, 4);
    std::memcpy(hdr + 40, &dataBytes, 4);
    std::fwrite(hdr, 1, 44, f);
    for (uint32_t i = 0; i < n; ++i) {
        auto clip = [](float v) {
            if (v > 1.0f) v = 1.0f; if (v < -1.0f) v = -1.0f;
            return (int16_t) (v * 32767.0f);
        };
        int16_t s[2] = { clip(l[i]), clip(r[i]) };
        std::fwrite(s, 2, 2, f);
    }
    std::fclose(f);
}

bool allFinite(const std::vector<float>& a, const std::vector<float>& b) {
    for (float v : a) if (!std::isfinite(v)) return false;
    for (float v : b) if (!std::isfinite(v)) return false;
    return true;
}

float peakOf(const std::vector<float>& a, const std::vector<float>& b) {
    float p = 0.0f;
    for (float v : a) { const float x = std::fabs(v); if (x > p) p = x; }
    for (float v : b) { const float x = std::fabs(v); if (x > p) p = x; }
    return p;
}

// RMS in dBFS over [fromSec, toSec)
float rmsDb(const std::vector<float>& x, double fromSec, double toSec) {
    size_t a = (size_t) (fromSec * kSR), b = (size_t) (toSec * kSR);
    if (b > x.size()) b = x.size();
    if (a >= b) return -160.0f;
    double acc = 0.0;
    for (size_t i = a; i < b; ++i) acc += (double) x[i] * x[i];
    const double rms = std::sqrt(acc / (double) (b - a));
    return rms > 1.0e-8 ? 20.0f * (float) std::log10(rms) : -160.0f;
}

// single-bin spectral probe (Goertzel), dB re full scale
float goertzelDb(const std::vector<float>& x, double hz, double fromSec, double toSec) {
    size_t a = (size_t) (fromSec * kSR), b = (size_t) (toSec * kSR);
    if (b > x.size()) b = x.size();
    if (a >= b) return -160.0f;
    const double w = 2.0 * 3.14159265358979 * hz / kSR;
    const double coef = 2.0 * std::cos(w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) {
        s0 = (double) x[i] + coef * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double n = (double) (b - a);
    const double power = (s1 * s1 + s2 * s2 - coef * s1 * s2) / (n * n / 4.0);
    return power > 1.0e-16 ? 10.0f * (float) std::log10(power) : -160.0f;
}

struct Render {
    std::vector<float> L, R;
};

struct Ev { double t; int ch; int note; float vel; bool on; };

// Render `seconds` of engine output with note events, free-running transport.
Render renderEvents(const EngineParams& p, const GroovePatterns& g,
                    double seconds, std::vector<Ev> evs) {
    GrooveEngine e;
    e.prepare(kSR, kBlock);
    e.setParams(p);
    e.setPatterns(g);
    const int total = (int) (seconds * kSR);
    Render out;
    out.L.resize((size_t) total);
    out.R.resize((size_t) total);
    TransportInfo t; // free-run
    int done = 0;
    size_t nextEv = 0;
    while (done < total) {
        const int n = total - done < kBlock ? total - done : kBlock;
        while (nextEv < evs.size() && (int) (evs[nextEv].t * kSR) <= done) {
            const Ev& ev = evs[nextEv];
            if (ev.on) e.noteOn(ev.ch, ev.note, ev.vel);
            else       e.noteOff(ev.ch, ev.note);
            ++nextEv;
        }
        e.process(out.L.data() + done, out.R.data() + done, n, t);
        done += n;
    }
    return out;
}

int gChecks = 0;
bool check(bool ok, const char* what) {
    ++gChecks;
    if (!ok) std::printf("  FAIL: %s\n", what);
    return ok;
}

// ------------------------------------------------------------------ cases --

// M0: a held bass note is audible, bounded, and dies after release.
int caseSmoke() {
    EngineParams p;
    GroovePatterns g;
    auto r = renderEvents(p, g, 6.0, {
        { 0.1, 1, 36, 0.9f, true  },
        { 3.0, 1, 36, 0.0f, false },
    });
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "smoke: finite");
    fails += !check(peakOf(r.L, r.R) <= 1.0f, "smoke: peak <= 1");
    const float sus = rmsDb(r.L, 0.5, 2.5);
    fails += !check(sus > -60.0f, "smoke: sustain > -60 dBFS");
    fails += !check(sus < -3.0f, "smoke: sustain upper bound");
    writeWav(gOutBase + "-smoke.wav", r.L, r.R);
    return fails;
}

// M0: output decays below -60 dBFS after note-off and stays there.
int caseRelease() {
    EngineParams p;
    GroovePatterns g;
    auto r = renderEvents(p, g, 8.0, {
        { 0.1, 1, 36, 0.9f, true  },
        { 2.0, 1, 36, 0.0f, false },
    });
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "release: finite");
    const float sus  = rmsDb(r.L, 0.5, 1.8);
    const float tail = rmsDb(r.L, 7.0, 8.0);
    fails += !check(tail < sus - 20.0f, "release: tail well below sustain");
    fails += !check(tail < -60.0f, "release: tail < -60 dBFS");
    writeWav(gOutBase + "-release.wav", r.L, r.R);
    return fails;
}

// A CBL-flavored 16-step bassline: root/fifth motion with rests and one slide.
GroovePatterns makeBassGroove() {
    GroovePatterns g;
    for (auto& s : g.bass.steps) s.gate = false;
    const auto set = [&](int ix, int note, bool acc, bool slide, int gateLen) {
        auto& s = g.bass.steps[ix];
        s.gate = true; s.note = note; s.accent = acc; s.slide = slide;
        s.gateLen = gateLen; s.slideT = 1; s.cvOct = 0.0f;
    };
    set(0, 33, true, false, 2);   // A1
    set(3, 33, false, false, 1);
    set(6, 40, false, false, 2);  // E2
    set(7, 38, false, true, 2);   // slide E->D
    set(10, 33, false, false, 2);
    set(12, 45, true, false, 1);  // A2 accent pop
    set(14, 31, false, false, 3); // G1 long
    return g;
}

// M1: sequenced bass, dry only — audible, bounded, sub fundamental present.
int caseBassIso() {
    EngineParams p;
    p.seq[0].on = true;
    p.seq[0].rate = 1; // 1/8 steps
    p.freeBpm = 100.0;
    auto r = renderEvents(p, makeBassGroove(), 10.0, {});
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "bass-iso: finite");
    fails += !check(peakOf(r.L, r.R) <= 1.0f, "bass-iso: peak <= 1");
    const float body = rmsDb(r.L, 1.0, 9.0);
    fails += !check(body > -40.0f, "bass-iso: sequenced bass audible");
    fails += !check(body < -6.0f, "bass-iso: upper RMS bound");
    // sub osc at -1 oct of A1 (55 Hz) = 27.5 Hz; fundamental at 55 Hz.
    const float subDb  = goertzelDb(r.L, 55.0, 1.0, 9.0);
    const float refDb  = goertzelDb(r.L, 5000.0, 1.0, 9.0);
    fails += !check(subDb > refDb + 12.0f, "bass-iso: sub-heavy spectrum");
    writeWav(gOutBase + "-bass-iso.wav", r.L, r.R);
    return fails;
}

// M1: delay send — echo lands at the synced division after the dry hit.
int caseSendDelay() {
    EngineParams p;
    p.mix.dlySend[0] = 1.0f;
    p.dly.ret = 1.0f;
    p.dly.feedback = 0.3f;
    p.dly.sync = true;
    p.dly.div = 4;         // 1 beat
    p.dly.mode = 0;        // plain stereo (simplest timing check)
    p.vrb.ret = 0.0f;
    p.freeBpm = 120.0;     // 1 beat = 0.5 s
    p.bass.vcaDecay = 0.1f; // short blip so echoes are separable
    // let the delay's time-glide settle before the hit
    auto r = renderEvents(p, {}, 5.0, {
        { 2.10, 1, 45, 0.9f, true  },
        { 2.16, 1, 45, 0.0f, false },
    });
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "send-delay: finite");
    const float echo1   = rmsDb(r.L, 2.60, 2.80); // hit + 0.5 s
    const float between = rmsDb(r.L, 2.40, 2.55);
    const float echo2   = rmsDb(r.L, 3.10, 3.30); // hit + 1.0 s
    fails += !check(echo1 > between + 10.0f, "send-delay: first echo present");
    fails += !check(echo2 > -70.0f, "send-delay: feedback repeats");
    fails += !check(echo2 < echo1, "send-delay: feedback decays");
    writeWav(gOutBase + "-send-delay.wav", r.L, r.R);
    return fails;
}

// M1: reverb send — Bloom tail sustains seconds after the note, then decays.
int caseSendReverb() {
    EngineParams p;
    p.mix.vrbSend[0] = 1.0f;
    p.vrb.ret = 1.0f;
    p.vrb.decay = 0.7f;    // T60 ~ 11 s
    p.dly.ret = 0.0f;
    p.bass.vcaDecay = 0.3f;
    auto r = renderEvents(p, {}, 8.0, {
        { 0.10, 1, 45, 0.9f, true  },
        { 0.60, 1, 45, 0.0f, false },
    });
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "send-reverb: finite");
    fails += !check(peakOf(r.L, r.R) <= 1.0f, "send-reverb: peak <= 1");
    const float early = rmsDb(r.L, 1.5, 2.5);
    const float tail  = rmsDb(r.L, 6.5, 8.0);
    fails += !check(tail > -60.0f, "send-reverb: tail alive at 6.5 s");
    fails += !check(tail < early, "send-reverb: tail decays");
    // stereo: the FDN taps decorrelate L/R
    double diff = 0.0, sum = 0.0;
    for (size_t i = (size_t) (2.0 * kSR); i < (size_t) (6.0 * kSR); ++i) {
        diff += (double) (r.L[i] - r.R[i]) * (r.L[i] - r.R[i]);
        sum  += (double) (r.L[i] + r.R[i]) * (r.L[i] + r.R[i]);
    }
    fails += !check(diff > sum * 0.01, "send-reverb: stereo tail");
    writeWav(gOutBase + "-send-reverb.wav", r.L, r.R);
    return fails;
}

// Count amplitude onsets: rising edges of a short-hop RMS envelope above
// threshold, with a refractory window. Returns onset sample positions.
std::vector<size_t> findOnsets(const std::vector<float>& x,
                               float thresholdDb = -35.0f,
                               double refractorySec = 0.15) {
    std::vector<size_t> onsets;
    const size_t hop = 128;
    const size_t refractory = (size_t) (refractorySec * kSR);
    const float th = std::pow(10.0f, thresholdDb / 20.0f);
    bool above = false;
    size_t lastOnset = 0;
    for (size_t i = 0; i + hop <= x.size(); i += hop) {
        double acc = 0.0;
        for (size_t j = i; j < i + hop; ++j) acc += (double) x[j] * x[j];
        const float rms = (float) std::sqrt(acc / (double) hop);
        if (!above && rms > th) {
            if (onsets.empty() || i - lastOnset > refractory) {
                onsets.push_back(i);
                lastOnset = i;
            }
            above = true;
        } else if (above && rms < th * 0.4f) {
            above = false;
        }
    }
    return onsets;
}

// Quarter-note blip pattern for clock tests.
GroovePatterns makeClickPattern() {
    GroovePatterns g;
    for (auto& s : g.bass.steps) {
        s.gate = true; s.note = 45; s.accent = false; s.slide = false;
        s.gateLen = 0; s.slideT = 0; s.cvOct = 0.0f;
    }
    return g;
}

EngineParams clickParams() {
    EngineParams p;
    p.seq[0].on = true;
    p.seq[0].rate = 4;       // 1/4 steps
    p.bass.vcaDecay = 0.05f; // short blips
    p.bass.subLevel = 0.0f;  // saw only: crisp onsets
    p.bass.cutoff = 0.8f;
    return p;
}

// M2: host transport — ppq loop jump resyncs without double-firing or NaN.
int caseTransportResync() {
    GrooveEngine e;
    e.prepare(kSR, kBlock);
    EngineParams p = clickParams();
    e.setParams(p);
    e.setPatterns(makeClickPattern());

    const double bpm = 120.0;                 // 1/4 step = 0.5 s
    const int total = (int) (8.0 * kSR);
    std::vector<float> L((size_t) total), Rr((size_t) total);
    TransportInfo t;
    t.bpm = bpm;
    t.playing = true;
    t.ppq = 0.0;
    int done = 0;
    while (done < total) {
        const int n = total - done < kBlock ? total - done : kBlock;
        // 4-beat loop in the host: ppq wraps back to 0 every 2 s
        const double rawPpq = (double) done / kSR * (bpm / 60.0);
        t.ppq = std::fmod(rawPpq, 4.0);
        e.process(L.data() + done, Rr.data() + done, n, t);
        done += n;
    }
    int fails = 0;
    fails += !check(allFinite(L, Rr), "transport-resync: finite");
    const auto onsets = findOnsets(L);
    // 8 s at 0.5 s per step = 16 steps; loop jumps land on step boundaries so
    // the count stays 16 (+-1 for edge effects)
    fails += !check(onsets.size() >= 15 && onsets.size() <= 17,
                    "transport-resync: no double/missed fires across loop jumps");
    writeWav(gOutBase + "-transport-resync.wav", L, Rr);
    return fails;
}

// M2: free-run — sequencer fires on the internal clock when host is stopped,
// step spacing matches the internal BPM.
int caseFreeRun() {
    EngineParams p = clickParams();
    p.freeBpm = 120.0;
    auto r = renderEvents(p, makeClickPattern(), 6.0, {});
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "free-run: finite");
    const auto onsets = findOnsets(r.L);
    fails += !check(onsets.size() >= 11 && onsets.size() <= 13,
                    "free-run: ~12 steps in 6 s @ 120 BPM");
    // spacing: every gap within 5 ms of 0.5 s
    bool spacingOk = onsets.size() >= 2;
    for (size_t i = 1; i < onsets.size(); ++i) {
        const double gap = (double) (onsets[i] - onsets[i - 1]) / kSR;
        if (std::fabs(gap - 0.5) > 0.005) spacingOk = false;
    }
    fails += !check(spacingOk, "free-run: step spacing 0.5 s +-5 ms");
    return fails;
}

// Slow CBL-style acid line: sparse, low, one slide pair, two accents.
GroovePatterns makeAcidGroove() {
    GroovePatterns g;
    for (auto& s : g.acid.steps) s.gate = false;
    const auto set = [&](int ix, int note, bool acc, bool slide) {
        auto& s = g.acid.steps[ix];
        s.gate = true; s.note = note; s.accent = acc; s.slide = slide;
    };
    set(0, 45, true, false);   // A2
    set(2, 45, false, false);
    set(5, 48, false, false);  // C3
    set(6, 50, false, true);   // slide C->D
    set(9, 45, false, false);
    set(12, 57, true, false);  // A3 accent
    set(13, 55, false, true);  // slide down
    return g;
}

// M3: acid solo — audible, bounded, and byte-identical across two renders
// (fixed seeds + shared clock => deterministic sequencing).
int caseAcidIso() {
    EngineParams p;
    p.seq[1].on = true;
    p.seq[1].rate = 0; // 1/16
    p.seq[1].dir = 3;  // RANDOM: exercises the seeded xorshift path too
    p.freeBpm = 100.0;
    auto g = makeAcidGroove();
    auto r1 = renderEvents(p, g, 8.0, {});
    auto r2 = renderEvents(p, g, 8.0, {});
    int fails = 0;
    fails += !check(allFinite(r1.L, r1.R), "acid-iso: finite");
    fails += !check(peakOf(r1.L, r1.R) <= 1.0f, "acid-iso: peak <= 1");
    const float body = rmsDb(r1.L, 1.0, 7.0);
    fails += !check(body > -40.0f && body < -6.0f, "acid-iso: RMS bounds");
    fails += !check(r1.L == r2.L && r1.R == r2.R, "acid-iso: byte-identical renders");
    writeWav(gOutBase + "-acid-iso.wav", r1.L, r1.R);
    return fails;
}

// M3: the accent circuit boosts level, and a slide step does not retrigger
// (no amplitude dip at the step boundary).
int caseAcidAccent() {
    EngineParams p;
    p.seq[1].on = true;
    p.seq[1].rate = 4; // 1/4 steps: separable hits
    p.freeBpm = 120.0;
    p.acid.accent = 1.0f; // full accent contrast
    p.acid.level = 0.45f; // keep the output tanh in its linear region
    GroovePatterns g;
    for (auto& s : g.acid.steps) s.gate = false;
    g.acid.steps[0] = { 45, 0, true, true,  false }; // accented
    g.acid.steps[2] = { 45, 0, true, false, false }; // plain
    g.acid.steps[4] = { 45, 0, true, false, false }; // -> slide pair
    g.acid.steps[5] = { 50, 0, true, false, true  }; // slides from step 4
    auto r = renderEvents(p, g, 4.0, {});
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "acid-accent: finite");
    // steps at 0.5 s spacing: accented hit at 0.0, plain at 1.0
    const float accPeak = rmsDb(r.L, 0.02, 0.20);
    const float plainPeak = rmsDb(r.L, 1.02, 1.20);
    fails += !check(accPeak > plainPeak + 2.0f, "acid-accent: accent louder");
    // slide: gate must hold across the step-4 -> step-5 boundary at 2.5 s
    const float atBoundary = rmsDb(r.L, 2.47, 2.53);
    fails += !check(atBoundary > -30.0f, "acid-accent: slide holds gate (no dip)");
    writeWav(gOutBase + "-acid-accent.wav", r.L, r.R);
    return fails;
}

// Always-on listening render: the current full instrument state, CBL-voiced.
// Lightly asserted — this case exists so every milestone leaves a WAV that
// answers "does it sound like the record yet?" (the walk-away test).
int caseDemo() {
    EngineParams p;
    p.seq[0].on = true;
    p.seq[0].rate = 1;      // 1/8
    p.seq[0].swing = 0.15f;
    p.freeBpm = 100.0;
    p.bass.cutoff = 0.32f;
    p.bass.res = 0.45f;
    p.bass.envMod = 0.5f;
    p.bass.subLevel = 0.8f;
    p.mix.dlySend[0] = 0.5f;
    p.mix.vrbSend[0] = 0.35f;
    p.seq[1].on = true;     // acid line over the bass
    p.seq[1].rate = 0;      // 1/16
    p.seq[1].swing = 0.15f;
    p.acid.cutoff = 0.25f;
    p.acid.res = 0.65f;
    p.acid.envMod = 0.4f;
    p.acid.octave = 0;
    p.mix.level[1] = 0.55f;
    p.mix.dlySend[1] = 0.7f;
    p.mix.vrbSend[1] = 0.3f;
    p.dly.sync = true;
    p.dly.div = 3;          // dotted 1/8
    p.dly.feedback = 0.55f;
    p.dly.mode = 1;         // ping-pong
    p.dly.toVerb = 0.3f;
    p.vrb.decay = 0.65f;
    p.vrb.shimmer = 0.25f;
    auto groove = makeBassGroove();
    groove.acid = makeAcidGroove().acid;
    auto r = renderEvents(p, groove, 20.0, {});
    int fails = 0;
    fails += !check(allFinite(r.L, r.R), "demo: finite");
    fails += !check(peakOf(r.L, r.R) <= 1.0f, "demo: peak <= 1");
    fails += !check(rmsDb(r.L, 2.0, 18.0) > -40.0f, "demo: audible");
    writeWav(gOutBase + "-demo.wav", r.L, r.R);
    return fails;
}

struct Case { const char* name; int (*fn)(); };
constexpr Case kCases[] = {
    { "smoke",       caseSmoke },
    { "release",     caseRelease },
    { "bass-iso",    caseBassIso },
    { "send-delay",  caseSendDelay },
    { "send-reverb", caseSendReverb },
    { "transport-resync", caseTransportResync },
    { "free-run",    caseFreeRun },
    { "acid-iso",    caseAcidIso },
    { "acid-accent", caseAcidAccent },
    { "demo",        caseDemo },
};

} // namespace

int main(int argc, char** argv) {
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--case") == 0 && i + 1 < argc) only = argv[++i];
        else if (argv[i][0] != '-') gOutBase = argv[i];
    }

    int fails = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (const auto& c : kCases) {
        if (only && std::strcmp(only, c.name) != 0) continue;
        std::printf("case %s\n", c.name);
        fails += c.fn();
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("%d checks, %d failures, %.0f ms wall\n", gChecks, fails, ms);

    if (fails > 0) {
        std::printf("RENDER TEST FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("RENDER TEST PASS\n");
    return 0;
}
