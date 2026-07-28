#pragma once
#include <cmath>

// Ported from abiogenesis/Source/engine/OscUtils.h (fleet shared).

namespace maru {

inline constexpr float kTwoPiF = 6.28318530717958647692f;

// polyBLEP residual for band-limiting discontinuities
inline float polyblep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float midiToHz(float note) {
    return 440.0f * std::exp2((note - 69.0f) / 12.0f);
}

} // namespace maru
