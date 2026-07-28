# Maru Mori

A psybient groovebox in the spirit of Carbon Based Lifeforms: four parts on one
sample-accurate clock — sub-heavy **Bass**, TB-303 **Acid**, 8-pad sample
**Drums**, and a juno-flavored poly **Pad** — mixed through per-part delay and
reverb sends into a tape-style stereo delay and the Bloom 8-line FDN reverb
(shimmer, freeze). Native macOS: Standalone app + VST3 + AU from one C++/JUCE
codebase. Part of the Arperture synth fleet.

## Build it

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # first run fetches JUCE
cmake --build build
```

All three formats build in one pass; the VST3 and AU auto-install to
`~/Library/Audio/Plug-Ins/`. Standalone:
`build/MaruMori_artefacts/Release/Standalone/Maru Mori.app`.
No Xcode needed — Command Line Tools + CMake + Ninja only.

## Play it

- Pick a factory preset (top right) — **Hydroponic Garden** is the intended
  first impression. Sequencers start with the host transport, or free-run on
  the internal BPM in standalone.
- The on-screen keyboard plays the part selected in **KEYS:** (bottom left).
  From a DAW / controller, each module listens on its own channel — the
  **MIDI CH** selector in its section (defaults: Bass 1, Acid 2, Drums 3
  with notes 36-43 = pads, Pad 4; two modules may share a channel to layer).
  Or hit the **CTRL** button on a module: your controller then drives that
  module regardless of channel — click again (or another module) to release.
  Sequenced mono parts ignore live notes while their sequencer runs; drums
  always layer.
- **Step grid**: tabs pick the part, BANK A-D picks the pattern (bank changes
  land on the next bar). Bass/Acid: drag NOTE, click GATE/ACC/SLIDE (bass
  adds gate-length, slide-time, and a filter-CV drag row). Drums: click =
  hit, right-click = accent. Pad: click a step to place a chord, again for
  tie, again for off; right-click cycles the chord; drag the chord tables.
- **Drums**: each pad ships with a built-in synthesized 808-ish sound; LOAD
  replaces it from disk (browser opens in the Audio Library's
  One-Shots/Drums & Percussion). CH and OH share choke group 1.
- **FX**: sends are per part in the MIXER. DELAY TO VERB bleeds echoes into
  the reverb tail — the Valhalla move. VERB FREEZE holds the tail forever
  (the shimmer governor keeps it stable).

## Panel walkthrough

| Section | Color | What it is |
|---|---|---|
| BASS | coral | Avalon Bassline voice: sub osc pre-filter, mod envelope with bipolar VCF/VCA depths (negative VCA MOD = drone), split accent/normal decays, log key tracking, 70 Hz/full range switch |
| ACID | green | TB-303: saw/shaped square, accent circuit with the resonance "wow", fixed 55 ms slides, accent ignores the decay knob (hardware behavior) |
| PAD | sky | 8-voice juno-ish poly: saw+PWM+sub, slow ADSR, LFO with delay ramp, BBD chorus |
| DRUMS | amber | 8 one-shot pads x tune/decay/cutoff/level/pan/choke + per-pad sample LOAD |
| SEQ | cream | Per part: on, rate (1/16..1/1 incl. triplets), length, direction (fwd/rev/pendulum/random), swing, bank |
| MIXER | cream | Per part: level, pan, delay send, reverb send |
| DELAY | lavender | Synced/free stereo/ping-pong/tape delay, tone-in-feedback, delay->verb feed |
| REVERB | lavender | Bloom FDN: decay to 45 s, size, damping, shimmer (+12/+7/+5/-12/dual), freeze |
| MASTER | cream | Gain, 18/70 Hz high-pass, internal BPM, global accent |

## Verify it

```bash
./build/MaruRenderTest renders/maru        # 17 offline cases, writes WAVs
# ASan/UBSan when DSP changed:
clang++ -std=c++20 -g -fsanitize=address,undefined -I Source \
  tests/RenderTest.cpp Source/engine/*.cpp Source/engine/parts/*.cpp \
  -o /tmp/rt_asan && /tmp/rt_asan /tmp/maru
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 8 \
  --validate ~/Library/Audio/Plug-Ins/VST3/"Maru Mori.vst3"
killall -9 AudioComponentRegistrar; auval -v aumu Maru Arpe
```

`renders/maru-demo.wav` is the standing walk-away test — the full instrument
playing the Hydroponic groove.

## Voicing

Every tune-by-ear constant lives in `Source/Tuning.h` and the `TUNE BY EAR`
blocks at the top of `AcidPart.cpp` / `BassPart.cpp` (ported verbatim from the
fleet's avalon/tb303 worklets — change them there knowingly). The one to
respect: `kBloomShimCeil` (0.35) — shimmer feedback runs away above ~0.5.

Fleet lineage: clock/sequencer semantics from Blacksite, Bloom/chorus/sampler
infrastructure from Abiogenesis, voices from the avalon/tb303/juno106 worklets.
