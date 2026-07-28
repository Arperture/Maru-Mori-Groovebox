#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Pre-M-final editor: generic parameter panel + keyboard (fleet law — custom
// UI effort stays at zero until the final milestone).
class MaruMoriEditor : public juce::AudioProcessorEditor {
public:
    explicit MaruMoriEditor(MaruMoriProcessor&);
    ~MaruMoriEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor panel;
    juce::MidiKeyboardComponent keyboard;
    juce::MidiDeviceListConnection midiDeviceConnection =
        juce::MidiDeviceListConnection::make([] {});

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaruMoriEditor)
};
