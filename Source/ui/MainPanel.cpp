#include "MainPanel.h"

namespace maru::ui {

MainPanel::MainPanel(MaruMoriProcessor& proc) : p(proc), presetMgr(proc) {
    // ---- sections (order defines nothing; bounds set in resized) ----
    addSection("BASS", pal::bass, 7, {
        "bassWave", "bassSubWave", "bassSubOct", "bassSubLevel", "bassCutoff",
        "bassRes", "bassEnvMod", "bassTracking", "bassDecay", "bassAccDecay",
        "bassAccent", "bassVcaDecay", "bassModAtt", "bassModDec", "bassVcfMod",
        "bassVcaMod", "bassGlide", "bassFr", "bassLevel", "bassMidiCh" });

    addSection("ACID", pal::acid, 4, {
        "acidWave", "acidCutoff", "acidRes", "acidEnvMod",
        "acidDecay", "acidAccent", "acidOctave", "acidLevel" });
    addWidget(*sections.back(), "acidMidiCh");

    addSection("PAD", pal::pad, 5, {
        "padPwm", "padSubLevel", "padCutoff", "padRes", "padAttack",
        "padDecayEnv", "padSustain", "padRelease", "padLfoRate", "padLfoDepth",
        "padLfoDelay", "padChorus", "padLevel" });
    addWidget(*sections.back(), "padMidiCh");

    buildDrumSection();

    addSection("SEQ", pal::mixer, 6, {
        "seqBassOn", "seqBassRate", "seqBassLen", "seqBassDir", "seqBassSwing", "seqBassBank",
        "seqAcidOn", "seqAcidRate", "seqAcidLen", "seqAcidDir", "seqAcidSwing", "seqAcidBank",
        "seqDrumOn", "seqDrumRate", "seqDrumLen", "seqDrumDir", "seqDrumSwing", "seqDrumBank",
        "seqPadOn",  "seqPadRate",  "seqPadLen",  "seqPadDir",  "seqPadSwing",  "seqPadBank" });

    addSection("MIXER", pal::mixer, 8, {
        "mixBassLvl", "mixBassPan", "mixBassDSend", "mixBassRSend",
        "mixAcidLvl", "mixAcidPan", "mixAcidDSend", "mixAcidRSend",
        "mixDrumLvl", "mixDrumPan", "mixDrumDSend", "mixDrumRSend",
        "mixPadLvl",  "mixPadPan",  "mixPadDSend",  "mixPadRSend" });

    addSection("DELAY", pal::fx, 4, {
        "dlySync", "dlyDiv", "dlyTime", "dlyMode",
        "dlyFb", "dlyTone", "dlyToVerb", "dlyRet" });

    addSection("REVERB", pal::fx, 5, {
        "vrbDecay", "vrbSize", "vrbPre", "vrbMod", "vrbLoDamp",
        "vrbHiDamp", "vrbShim", "vrbShimInt", "vrbFreeze", "vrbRet" });

    addSection("MASTER", pal::master, 4, {
        "masterGain", "masterHp", "masterBpm", "masterAccent" });

    stepGrid = std::make_unique<StepGrid>(p);
    addAndMakeVisible(*stepGrid);

    buildPresetBar();
    buildCtrlButtons();
    startTimer(250);
}

void MainPanel::buildCtrlButtons() {
    static const juce::Colour cols[4] = { pal::bass, pal::acid, pal::drums, pal::pad };
    for (int i = 0; i < 4; ++i) {
        auto& b = ctrlButtons[(size_t) i];
        b.setButtonText("CTRL");
        b.setTooltip("Focus your MIDI controller on this module "
                     "(any channel). Click again for per-channel routing.");
        b.setColour(juce::TextButton::textColourOffId, cols[i]);
        addAndMakeVisible(b);
        b.onClick = [this, i] {
            if (auto* prm = p.apvts.getParameter("midiFocus")) {
                const int cur = (int) prm->convertFrom0to1(prm->getValue());
                const int next = cur == i + 1 ? 0 : i + 1; // toggle / radio
                prm->setValueNotifyingHost(prm->convertTo0to1((float) next));
            }
        };
    }
}

void MainPanel::timerCallback() {
    int focus = 0;
    if (auto* v = p.apvts.getRawParameterValue("midiFocus"))
        focus = (int) v->load();
    static const juce::Colour cols[4] = { pal::bass, pal::acid, pal::drums, pal::pad };
    for (int i = 0; i < 4; ++i) {
        auto& b = ctrlButtons[(size_t) i];
        const bool on = focus == i + 1;
        b.setColour(juce::TextButton::buttonColourId,
                    on ? cols[i] : pal::chassis.brighter(0.1f));
        b.setColour(juce::TextButton::textColourOffId,
                    on ? pal::chassis : cols[i]);
    }
}

// ---------------------------------------------------------------------------

MainPanel::Section& MainPanel::addSection(const juce::String& title,
                                          juce::Colour accent, int cols,
                                          std::initializer_list<const char*> ids) {
    auto section = std::make_unique<Section>();
    section->title = title;
    section->accent = accent;
    section->cols = cols;
    for (auto* id : ids)
        addWidget(*section, id);
    sections.push_back(std::move(section));
    return *sections.back();
}

void MainPanel::addWidget(Section& s, const char* paramId) {
    auto* param = p.apvts.getParameter(paramId);
    if (param == nullptr) { jassertfalse; return; } // typo'd id
    Widget w;

    if (auto* pb = dynamic_cast<juce::AudioParameterBool*>(param)) {
        auto b = std::make_unique<juce::ToggleButton>(
            param->getName(12).toUpperCase());
        b->setColour(juce::TextButton::buttonOnColourId, s.accent);
        w.buttonAtt = std::make_unique<juce::ButtonParameterAttachment>(*pb, *b);
        addAndMakeVisible(*b);
        w.comp = std::move(b);
    } else if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(param)) {
        auto c = std::make_unique<juce::ComboBox>();
        c->addItemList(pc->choices, 1);
        c->setColour(juce::ComboBox::arrowColourId, s.accent);
        w.comboAtt = std::make_unique<juce::ComboBoxParameterAttachment>(*pc, *c);
        addAndMakeVisible(*c);
        w.comp = std::move(c);
        w.label = std::make_unique<juce::Label>(juce::String(),
            param->getName(14).toUpperCase());
    } else {
        auto sl = std::make_unique<juce::Slider>(
            juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        sl->setColour(juce::Slider::rotarySliderFillColourId, s.accent);
        sl->setPopupDisplayEnabled(true, true, this);
        w.sliderAtt = std::make_unique<juce::SliderParameterAttachment>(
            *dynamic_cast<juce::RangedAudioParameter*>(param), *sl);
        addAndMakeVisible(*sl);
        w.comp = std::move(sl);
        w.label = std::make_unique<juce::Label>(juce::String(),
            param->getName(14).toUpperCase());
    }
    if (w.label != nullptr) {
        w.label->setJustificationType(juce::Justification::centred);
        w.label->setColour(juce::Label::textColourId, pal::textDim);
        addAndMakeVisible(*w.label);
    }
    s.widgets.push_back(std::move(w));
}

void MainPanel::buildDrumSection() {
    auto& s = addSection("DRUMS", pal::drums, 8, {});
    static const char* kSuffix[6] = { "Tune", "Decay", "Cut", "Lvl", "Pan", "Choke" };
    for (int pad = 0; pad < 8; ++pad)
        for (auto* suf : kSuffix)
            addWidget(s, ("drum" + juce::String(pad + 1) + suf).toRawUTF8());
    addWidget(s, "drumMidiCh"); // laid out beside the CTRL button in resized()
    drumSection = sections.back().get();

    for (int pad = 0; pad < 8; ++pad) {
        auto& b = loadButtons[(size_t) pad];
        b.setButtonText("LOAD");
        b.setColour(juce::TextButton::buttonColourId, pal::block(pal::drums));
        addAndMakeVisible(b);
        b.onClick = [this, pad] {
            // right place for Drew's library: One-Shots/Drums & Percussion
            juce::File start("/Volumes/Primary/Arperture/Audio Library/One-Shots/Drums & Percussion");
            if (!start.isDirectory())
                start = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
            chooser = std::make_unique<juce::FileChooser>(
                "Load sample for pad " + juce::String(pad + 1),
                start, "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync(juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles,
                [this, pad](const juce::FileChooser& fc) {
                    const auto f = fc.getResult();
                    if (f.existsAsFile()) {
                        p.drumStores.load(pad, f);
                        padPathLabels[(size_t) pad].setText(
                            f.getFileNameWithoutExtension(),
                            juce::dontSendNotification);
                    }
                });
        };
        auto& l = padPathLabels[(size_t) pad];
        l.setText("(built-in)", juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, pal::textDim);
        addAndMakeVisible(l);
    }
}

void MainPanel::buildPresetBar() {
    addAndMakeVisible(presetBox);
    presetBox.setTextWhenNothingSelected("PRESETS");
    refreshPresetBox();
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        if (id >= 1 && id <= presets::kNumFactory) {
            presetMgr.loadFactory(id - 1);
        } else if (id >= 1000) {
            const auto files = presetMgr.userPresets();
            const int ix = id - 1000;
            if (ix < files.size())
                presetMgr.loadUser(files[ix]);
        }
    };

    addAndMakeVisible(saveButton);
    saveButton.onClick = [this] {
        auto* window = new juce::AlertWindow("Save preset",
            "Name this preset:", juce::MessageBoxIconType::NoIcon);
        window->addTextEditor("name", "My Groove");
        window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        window->enterModalState(true,
            juce::ModalCallbackFunction::create([this, window](int result) {
                if (result == 1)
                    presetMgr.saveUser(window->getTextEditorContents("name"));
                refreshPresetBox();
                delete window;
            }));
    };
}

void MainPanel::refreshPresetBox() {
    presetBox.clear(juce::dontSendNotification);
    for (int i = 0; i < presets::kNumFactory; ++i)
        presetBox.addItem(presets::kFactory[i].name, i + 1);
    const auto files = presetMgr.userPresets();
    if (!files.isEmpty())
        presetBox.addSeparator();
    for (int i = 0; i < files.size(); ++i)
        presetBox.addItem(files[i].getFileNameWithoutExtension(), 1000 + i);
}

// ---------------------------------------------------------------------------

void MainPanel::paint(juce::Graphics& g) {
    g.fillAll(pal::chassis);

    g.setColour(pal::text);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("MARU MORI", 12, 6, 220, 26, juce::Justification::centredLeft);
    g.setColour(pal::textDim);
    g.setFont(11.0f);
    g.drawText("ARPERTURE  |  psybient groovebox", 176, 10, 260, 18,
               juce::Justification::centredLeft);

    for (auto& sp : sections) {
        auto& s = *sp;
        g.setColour(pal::block(s.accent));
        g.fillRoundedRectangle(s.bounds.toFloat(), 6.0f);
        g.setColour(s.accent.withAlpha(0.5f));
        g.drawRoundedRectangle(s.bounds.toFloat().reduced(0.5f), 6.0f, 1.0f);
        g.setColour(s.accent);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(s.title, s.bounds.getX() + 10, s.bounds.getY() + 4,
                   200, 14, juce::Justification::centredLeft);
    }

    if (drumSection != nullptr) {
        g.setColour(pal::textDim);
        g.setFont(10.0f);
        static const char* names[8] = { "KICK", "SNARE", "CLAP", "CH",
                                        "OH", "TOM", "RIM", "SHAKER" };
        const auto r = drumSection->bounds;
        const int colW = r.getWidth() / 8;
        for (int pad = 0; pad < 8; ++pad) {
            g.setColour(pal::drums);
            g.drawText(names[pad], r.getX() + pad * colW, r.getY() + 4,
                       colW, 12, juce::Justification::centred);
        }
    }
}

void MainPanel::layoutSection(Section& s) {
    auto area = s.bounds.reduced(8).withTrimmedTop(16);
    const int n = (int) s.widgets.size();
    if (n == 0) return;
    const int rows = (n + s.cols - 1) / s.cols;
    const int cw = area.getWidth() / s.cols;
    const int chh = area.getHeight() / rows;
    for (int i = 0; i < n; ++i) {
        auto cellArea = juce::Rectangle<int>(
            area.getX() + (i % s.cols) * cw,
            area.getY() + (i / s.cols) * chh, cw, chh).reduced(3);
        auto& w = s.widgets[(size_t) i];
        if (w.label != nullptr) {
            w.label->setBounds(cellArea.removeFromBottom(13));
            if (dynamic_cast<juce::ComboBox*>(w.comp.get()) != nullptr)
                w.comp->setBounds(cellArea.withSizeKeepingCentre(
                    cellArea.getWidth(), juce::jmin(24, cellArea.getHeight())));
            else
                w.comp->setBounds(cellArea);
        } else {
            w.comp->setBounds(cellArea);
        }
    }
}

void MainPanel::resized() {
    // header
    presetBox.setBounds(getWidth() - 320, 6, 220, 24);
    saveButton.setBounds(getWidth() - 92, 6, 80, 24);

    const auto find = [this](const juce::String& t) -> Section* {
        for (auto& s : sections)
            if (s->title == t) return s.get();
        return nullptr;
    };

    if (auto* s = find("BASS"))   s->bounds = { 8, 36, 650, 216 };
    if (auto* s = find("ACID"))   s->bounds = { 666, 36, 380, 216 };
    if (auto* s = find("PAD"))    s->bounds = { 1054, 36, 438, 216 };
    if (auto* s = find("DRUMS"))  s->bounds = { 8, 260, 1484, 208 };

    // CTRL focus buttons live in the part sections' title bars
    {
        const char* names[4] = { "BASS", "ACID", "DRUMS", "PAD" };
        for (int i = 0; i < 4; ++i)
            if (auto* s = find(names[i]))
                ctrlButtons[(size_t) i].setBounds(
                    s->bounds.getRight() - 62, s->bounds.getY() + 3, 54, 17);
    }
    if (auto* s = find("SEQ"))    s->bounds = { 8, 476, 560, 216 };
    stepGrid->setBounds(576, 476, 916, 216);
    if (auto* s = find("MIXER"))  s->bounds = { 8, 700, 700, 156 };
    if (auto* s = find("DELAY"))  s->bounds = { 716, 700, 380, 156 };
    if (auto* s = find("REVERB")) s->bounds = { 1104, 700, 388, 156 };
    if (auto* s = find("MASTER")) s->bounds = { 8, 864, 360, 104 };

    for (auto& sp : sections)
        if (sp.get() != drumSection)
            layoutSection(*sp);

    // drums: 8 pad columns, each = LOAD + name + 6 knobs in 2 rows of 3;
    // widget 48 is the drumMidiCh combo, parked in the title bar
    if (drumSection != nullptr) {
        if (drumSection->widgets.size() > 48) {
            auto& w = drumSection->widgets[48];
            const auto r = drumSection->bounds;
            w.comp->setBounds(r.getRight() - 130, r.getY() + 2, 60, 19);
            if (w.label != nullptr)
                w.label->setBounds(r.getRight() - 202, r.getY() + 4, 70, 15);
        }
        const auto r = drumSection->bounds;
        const int colW = r.getWidth() / 8;
        for (int pad = 0; pad < 8; ++pad) {
            juce::Rectangle<int> col(r.getX() + pad * colW, r.getY() + 16,
                                     colW, r.getHeight() - 16);
            auto top = col.removeFromTop(20).reduced(8, 0);
            loadButtons[(size_t) pad].setBounds(top.removeFromLeft(52));
            padPathLabels[(size_t) pad].setBounds(top);
            const int kw = col.getWidth() / 3;
            const int kh = (col.getHeight() - 4) / 2;
            for (int k = 0; k < 6; ++k) {
                if ((size_t) (pad * 6 + k) >= 48) continue; // 48 = drumMidiCh
                auto& w = drumSection->widgets[(size_t) (pad * 6 + k)];
                auto cellArea = juce::Rectangle<int>(
                    col.getX() + (k % 3) * kw,
                    col.getY() + (k / 3) * kh, kw, kh).reduced(2);
                if (w.label != nullptr) {
                    w.label->setBounds(cellArea.removeFromBottom(11));
                    if (dynamic_cast<juce::ComboBox*>(w.comp.get()) != nullptr)
                        cellArea = cellArea.withSizeKeepingCentre(
                            cellArea.getWidth(), juce::jmin(20, cellArea.getHeight()));
                }
                w.comp->setBounds(cellArea);
            }
        }
    }
}

} // namespace maru::ui
