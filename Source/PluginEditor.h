#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ui/MaruLookAndFeel.h"
#include "ui/MainPanel.h"

class MaruMoriEditor : public juce::AudioProcessorEditor {
public:
    explicit MaruMoriEditor(MaruMoriProcessor&);
    ~MaruMoriEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    maru::ui::MaruLookAndFeel lnf;
    maru::ui::MainPanel panel;
    juce::MidiKeyboardComponent keyboard;
    juce::ComboBox playTarget; // which part the on-screen keys drive (ch 1-4)
    juce::MidiDeviceListConnection midiDeviceConnection =
        juce::MidiDeviceListConnection::make([] {});

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaruMoriEditor)
};
