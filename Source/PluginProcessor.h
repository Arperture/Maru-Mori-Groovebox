#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "engine/GrooveEngine.h"
#include "sampler/SampleStore.h"

class MaruMoriProcessor : public juce::AudioProcessor {
public:
    MaruMoriProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // Pattern data: authoritative copy for the audio thread, serialized into
    // the GROOVE state ValueTree side-block (never as automatable params).
    // Message thread edits under grooveLock; processBlock try-locks and copies.
    void setGrooveFromUi(const maru::GrooveBanks& g);
    maru::GrooveBanks getGroove() const;

    // drum sample slots (message-thread loads, RT-safe handoff)
    maru::DrumSampleStores drumStores;

private:
    void handleMidiEvent(const juce::MidiMessage& m);
    void loadGrooveFromState();
    void writeGrooveToState();

    maru::params::ParamRefs paramRefs;
    maru::GrooveEngine engine;

    mutable juce::SpinLock grooveLock;
    maru::GrooveBanks groove;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaruMoriProcessor)
};
