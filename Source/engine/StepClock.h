#pragma once
#include <cmath>

namespace maru {

// Per-sample step derivation on the shared musical clock, factored from
// Blacksite Engine.cpp (fleet reference). Swing defers odd steps: before the
// swing offset has elapsed they still belong to the previous step.
struct StepClock {
    long long lastStep = kNever;

    static constexpr long long kNever = -0x7fffffffffffffLL;

    void reset() { lastStep = kNever; }

    // Returns the absolute step number that fires at this musical position,
    // or -1 if no new step boundary was crossed.
    long long tick(double musicalPos, double stepBeats, double swingOff) {
        long long n = (long long) std::floor(musicalPos / stepBeats);
        if ((n & 1) && (musicalPos - (double) n * stepBeats) < swingOff)
            --n;
        if (n == lastStep)
            return -1;
        lastStep = n;
        return n;
    }
};

} // namespace maru
