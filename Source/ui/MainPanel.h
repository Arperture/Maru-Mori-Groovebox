#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../PluginProcessor.h"
#include "../presets/PresetManager.h"
#include "MaruLookAndFeel.h"
#include "StepGrid.h"

namespace maru::ui {

// Moog-Grandmother-style color-blocked panel: each part lives on its own
// tinted section. Widgets are auto-built from param IDs by type (abio
// MainPanel pattern) — bool -> toggle, choice -> combo, float -> rotary.
class MainPanel : public juce::Component {
public:
    static constexpr int kWidth = 1500;
    static constexpr int kHeight = 980;

    explicit MainPanel(MaruMoriProcessor&);
    ~MainPanel() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Widget {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::SliderParameterAttachment> sliderAtt;
        std::unique_ptr<juce::ButtonParameterAttachment> buttonAtt;
        std::unique_ptr<juce::ComboBoxParameterAttachment> comboAtt;
    };
    struct Section {
        juce::String title;
        juce::Colour accent;
        int cols = 4;
        juce::Rectangle<int> bounds;
        std::vector<Widget> widgets;
    };

    Section& addSection(const juce::String& title, juce::Colour accent, int cols,
                        std::initializer_list<const char*> paramIds);
    void addWidget(Section& s, const char* paramId);
    void layoutSection(Section& s);
    void buildDrumSection();
    void buildPresetBar();
    void refreshPresetBox();

    MaruMoriProcessor& p;
    presets::PresetManager presetMgr;
    std::vector<std::unique_ptr<Section>> sections;
    Section* drumSection = nullptr; // custom layout: 8 pad columns
    std::unique_ptr<StepGrid> stepGrid;

    // header
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" };
    std::array<juce::TextButton, 8> loadButtons; // per drum pad
    std::array<juce::Label, 8> padPathLabels;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace maru::ui
