#pragma once
#include <vector>
#include <cmath>

// Ported from Blacksite/Source/engine/fx/DelayLine.h (which carries the
// readFrac float-wrap fix — the abiogenesis ASan lesson).

namespace maru {

// Integer-index delay line + lattice allpass. Sized in prepare(); never
// resized on the audio thread.
class DelayLine {
public:
    void resize(int n) {
        buf.assign((size_t) (n < 2 ? 2 : n), 0.0f);
        i = 0;
    }

    float read(int offset) const {
        int n = (int) buf.size();
        int p = i - offset;
        p = ((p % n) + n) % n;
        return buf[(size_t) p];
    }

    // fractional-delay linear-interp read (for modulated taps)
    float readFrac(float delay) const {
        int n = (int) buf.size();
        float pos = (float) i - delay;
        pos = std::fmod(pos, (float) n);
        if (pos < 0.0f) pos += (float) n;
        // float edge: pos += n can round to exactly n — wrap before indexing
        if (pos >= (float) n) pos -= (float) n;
        int i0 = (int) pos;
        if (i0 >= n) i0 = n - 1;
        float frac = pos - (float) i0;
        int i1 = i0 + 1;
        if (i1 >= n) i1 = 0;
        return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
    }

    float tail() const { return buf[(size_t) i]; }

    void push(float v) {
        buf[(size_t) i] = v;
        i = (i + 1) % (int) buf.size();
    }

    int size() const { return (int) buf.size(); }

private:
    std::vector<float> buf { 0.0f, 0.0f };
    int i = 0;
};

class Allpass {
public:
    void resize(int n, float gain) {
        dl.resize(n);
        g = gain;
    }

    float process(float x) {
        // lattice allpass: v[n] = x + g*v[n-N]; y = v[n-N] - g*v[n]
        float vOld = dl.tail();
        float vNew = x + g * vOld;
        dl.push(vNew);
        return vOld - g * vNew;
    }

    float read(int offset) const { return dl.read(offset); }

private:
    DelayLine dl;
    float g = 0.5f;
};

} // namespace maru
