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

struct Case { const char* name; int (*fn)(); };
constexpr Case kCases[] = {
    { "smoke",   caseSmoke },
    { "release", caseRelease },
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
