#pragma once

namespace maru {

// Per-block (per-segment) clock snapshot handed to every part. Each part
// derives its own step boundaries per-sample from the same arithmetic, so all
// four sequencers stay sample-aligned with zero coupling between parts.
struct ClockState {
    double blockStartBeat = 0.0; // musicalPos at sample 0 of this segment
    double beatsPerSample = 0.0;
    double bpm            = 120.0;
    bool   playing        = false;

    double beatAt(int sample) const { return blockStartBeat + sample * beatsPerSample; }
};

} // namespace maru
