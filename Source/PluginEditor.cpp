#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

// JUCE's desktop standalone ships with shouldAutoOpenMidiDevices = false, so
// hardware controllers are silent until manually ticked in Options. An
// instrument should just listen: enable every input (no-op in DAW wrappers,
// where MIDI arrives from the host instead). Fleet-standard fix.
static void enableAllStandaloneMidiInputs() {
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        for (const auto& device : juce::MidiInput::getAvailableDevices())
            holder->deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
#endif
}

MaruMoriEditor::MaruMoriEditor(MaruMoriProcessor& p)
    : AudioProcessorEditor(p),
      panel(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
    addAndMakeVisible(panel);
    addAndMakeVisible(keyboard);
    keyboard.setLowestVisibleKey(24);
    keyboard.setKeyWidth(22.0f);
    setSize(900, 700);

    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
        // async: the standalone window registers itself with the Desktop after
        // constructing this editor, and getInstance() scans Desktop windows
        juce::MessageManager::callAsync([] { enableAllStandaloneMidiInputs(); });
        midiDeviceConnection = juce::MidiDeviceListConnection::make(
            [] { enableAllStandaloneMidiInputs(); }); // hot-plug coverage
    }
}

void MaruMoriEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff12161d));
}

void MaruMoriEditor::resized() {
    auto area = getLocalBounds();
    keyboard.setBounds(area.removeFromBottom(64).reduced(8, 4));
    panel.setBounds(area);
}
