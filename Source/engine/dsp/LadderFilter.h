#pragma once
#include <cmath>

// 4-pole TPT/ZDF ladder with tanh drive, ported from the fleet juno106 worklet
// via Blacksite/Source/engine/LadderFilter.h (WARHEAD stage dropped — Maru Mori
// keeps the clean fleet ladder; the resonance feedback is still tanh-guarded).

namespace maru {

struct LadderFilter {
    float z1 = 0.0f, z2 = 0.0f, z3 = 0.0f, z4 = 0.0f;

    void reset() { z1 = z2 = z3 = z4 = 0.0f; }

    // x: input sample (already drive-saturated), g = tan(pi * fc / sr),
    // k: resonance feedback gain
    float process(float x, float g, float k) {
        const float G  = g / (1.0f + g);
        const float G2 = G * G;
        const float S  = (G2 * G * z1 + G2 * z2 + G * z3 + z4) / (1.0f + g);

        float u = (x - k * S) / (1.0f + k * G2 * G2);
        u = std::tanh(u);
        float a = G * (u - z1);  const float y1 = a + z1;  z1 = y1 + a;
        a = G * (y1 - z2);       const float y2 = a + z2;  z2 = y2 + a;
        a = G * (y2 - z3);       const float y3 = a + z3;  z3 = y3 + a;
        a = G * (y3 - z4);       const float y4 = a + z4;  z4 = y4 + a;
        return y4;
    }
};

} // namespace maru
