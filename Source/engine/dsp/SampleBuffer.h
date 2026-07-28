#pragma once
#include <vector>
#include <cstdint>

// Ported from abiogenesis/Source/engine/sampler/SampleBuffer.h (POD only —
// the looping SamplerVoice stays behind; drums use OneShotSampler).

namespace maru {

// Plain sample data crossing the plugin -> engine boundary. Built and owned by
// the JUCE-side SampleStore (or the engine's DefaultKit); the engine only ever
// holds a const pointer that stays valid for at least the block it was
// snapshotted in (generation GC).
struct SampleBuffer {
    std::vector<float> left, right;   // mono files duplicated into both
    double   sourceRate = 48000.0;
    int      numFrames  = 0;
    uint64_t generation = 0;
};

} // namespace maru
