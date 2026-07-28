#pragma once

// ===== TUNE BY EAR =====
// Voicing constants for Maru Mori. Adjust by listening, fleet convention.
// Every feedback loop's hard ceiling lives here (fleet law).

namespace maru::tune {

// -- sizing / rates --
inline constexpr int   kPadVoices       = 8;    // pad part polyphony
inline constexpr int   kDrumPads        = 8;    // one-shot sample slots
inline constexpr int   kControlInterval = 16;   // samples between control-rate updates

// -- envelopes (AmbientADSR, pad part) --
inline constexpr float kAttackOvershoot = 1.6f;   // RC attack chases past 1.0
inline constexpr float kEnvIdleFloor    = 1.0e-4f; // voice reclaim threshold (~-80 dB)

// -- bass part (avalon port) --
inline constexpr float kBassCutoffMinHz = 40.0f;
inline constexpr float kBassCutoffMaxHz = 8000.0f;
inline constexpr float kBassResMaxK     = 4.05f;  // ladder feedback at full resonance
inline constexpr float kBassDrive       = 1.05f;  // pre-filter drive
inline constexpr float kBassEnvOctaves  = 4.0f;   // mod env -> cutoff sweep range
inline constexpr float kSubLevelGain    = 0.85f;  // avalon SUB_LEVEL_GAIN
inline constexpr float kBassOutTrim     = 0.65f;
inline constexpr float kBassVcaAttack   = 0.003f; // seconds
inline constexpr float kBassVelFloor    = 0.30f;

// -- sequencer --
inline constexpr double kSeqSwingMax    = 1.0 / 3.0; // fraction of a step, Blacksite law
inline constexpr float  kSeqAccentVel   = 1.0f;
inline constexpr float  kSeqPlainVel    = 0.72f;

// -- mixer / master --
inline constexpr float kMixSmoothTau    = 0.010f; // 10 ms level/pan smoothing
inline constexpr float kMasterGuardDrive = 0.6f;  // tanh knee: unity to ~-6 dBFS
inline constexpr float kMasterHpLoHz    = 18.0f;  // full-range mode (CBL sub default)
inline constexpr float kMasterHpHiHz    = 70.0f;  // 303-style HP mode

// -- send FX ceilings (fleet law: every feedback loop gets one) --
inline constexpr float kSendDrive       = 0.7f;   // tanh drive into send FX inputs
inline constexpr float kDlyToVerbMax    = 0.6f;   // delay output -> reverb feed cap
inline constexpr float kBloomShimCeil   = 0.35f;  // shimmer feedback — runs away above ~0.5

// -- misc --
inline constexpr float kDenormGuard     = 1.0e-20f;

} // namespace maru::tune
