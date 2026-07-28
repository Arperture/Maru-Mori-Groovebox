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
    setLookAndFeel(&lnf);
    addAndMakeVisible(panel);
    addAndMakeVisible(keyboard);
    keyboard.setLowestVisibleKey(24);
    keyboard.setKeyWidth(22.0f);

    addAndMakeVisible(playTarget);
    playTarget.addItemList({ "KEYS: BASS", "KEYS: ACID", "KEYS: DRUMS", "KEYS: PAD" }, 1);
    playTarget.setSelectedId(1, juce::dontSendNotification);
    playTarget.onChange = [this] {
        keyboard.setMidiChannel(playTarget.getSelectedId()); // ch 1-4 -> part
    };

    setSize(maru::ui::MainPanel::kWidth, maru::ui::MainPanel::kHeight + 72);

    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
        // async: the standalone window registers itself with the Desktop after
        // constructing this editor, and getInstance() scans Desktop windows
        juce::MessageManager::callAsync([] { enableAllStandaloneMidiInputs(); });
        midiDeviceConnection = juce::MidiDeviceListConnection::make(
            [] { enableAllStandaloneMidiInputs(); }); // hot-plug coverage
    }
}

MaruMoriEditor::~MaruMoriEditor() {
    setLookAndFeel(nullptr);
}

void MaruMoriEditor::paint(juce::Graphics& g) {
    g.fillAll(maru::ui::pal::chassis);
}

void MaruMoriEditor::resized() {
    auto area = getLocalBounds();
    auto bottom = area.removeFromBottom(72);
    playTarget.setBounds(bottom.removeFromLeft(130).reduced(8, 22));
    keyboard.setBounds(bottom.reduced(8, 4));
    panel.setBounds(area);
}
