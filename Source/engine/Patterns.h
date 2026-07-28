#pragma once

namespace maru {

// All pattern data is plain POD, NOT automatable parameters (fleet law).
// The authoritative copies live in the processor guarded by a SpinLock;
// processBlock copies them into the engine under a non-blocking try-lock and
// they serialize into the <GROOVE> state ValueTree side-block.

// Acid: Blacksite SeqStep, unchanged semantics.
struct AcidStep {
    int  note   = 36;
    int  oct    = 0;     // -1 / 0 / +1
    bool gate   = true;
    bool accent = false;
    bool slide  = false;
};
struct AcidPattern { AcidStep steps[16]; };

// Bass: acid step + avalon depth (per-step gate length, slide time, filter CV).
struct BassStep {
    int   note    = 36;
    int   oct     = 0;
    bool  gate    = true;
    bool  accent  = false;
    bool  slide   = false;
    int   gateLen = 2;    // index into kGateFracs {0.10, 0.30, 0.50, 0.90}
    int   slideT  = 1;    // index into kSlideTimes {0.03, 0.10, 0.25, 1.0} s
    float cvOct   = 0.0f; // per-step filter CV in octaves, +-4 range
};
struct BassPattern { BassStep steps[16]; };

// Drums: 8 lanes x 16 cells.
struct DrumCell { bool on = false; bool accent = false; };
struct DrumGrid { DrumCell cells[8][16]; };

// Pad: chord-slot step sequencer. Each gated step triggers one of 4 editable
// chords; tie holds the previous chord through this step without retriggering.
struct PadChord {
    int notes[4] = { 57, 60, 64, 67 }; // A minor add nothing — placeholder voicing
    int count    = 4;
};
struct PadStep  { bool gate = false; int chord = 0; bool tie = false; };
struct PadPattern {
    PadStep  steps[16];
    PadChord chords[4];
};

struct GroovePatterns {
    BassPattern bass;
    AcidPattern acid;
    DrumGrid    drums;
    PadPattern  pad;
};

// Four banks (A-D) per part. Bank selection is a parameter; the engine
// applies a requested bank change at the next bar boundary (queued switch).
inline constexpr int kNumBanks = 4;
struct GrooveBanks {
    BassPattern bass[kNumBanks];
    AcidPattern acid[kNumBanks];
    DrumGrid    drums[kNumBanks];
    PadPattern  pad[kNumBanks];
};

// Avalon per-step tables (shared by bass part; acid uses fixed 303 gate frac)
inline constexpr float kGateFracs[4]  = { 0.10f, 0.30f, 0.50f, 0.90f };
inline constexpr float kSlideTimes[4] = { 0.03f, 0.10f, 0.25f, 1.0f };

} // namespace maru
