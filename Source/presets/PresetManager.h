#pragma once
#include "../PluginProcessor.h"
#include "FactoryPresets.h"

namespace maru::presets {

// Factory bank + user presets. Factory: reset every param to default, apply
// the preset's KV overrides, rebuild the groove banks. User presets: the full
// processor state blob written to ~/Library/Application Support/Arperture/
// Maru Mori/Presets/<name>.marupreset (params + GROOVE + DRUMKIT paths).
class PresetManager {
public:
    explicit PresetManager(MaruMoriProcessor& proc) : p(proc) {}

    static juce::File userDir() {
        return juce::File::getSpecialLocation(
                   juce::File::userApplicationDataDirectory)
            .getChildFile("Arperture").getChildFile("Maru Mori")
            .getChildFile("Presets");
    }

    void loadFactory(int index) {
        if (index < 0 || index >= kNumFactory) return;
        const FactoryPreset& fp = kFactory[index];

        // reset everything to defaults first — presets are absolute
        for (auto* param : p.getParameters())
            if (auto* withId = dynamic_cast<juce::RangedAudioParameter*>(param))
                withId->setValueNotifyingHost(withId->getDefaultValue());

        for (int i = 0; i < fp.n; ++i) {
            if (auto* prm = p.apvts.getParameter(fp.kv[i].id)) {
                prm->setValueNotifyingHost(prm->convertTo0to1(fp.kv[i].v));
            } else {
                jassertfalse; // typo'd param id in a factory preset
            }
        }

        if (fp.buildGroove != nullptr) {
            GrooveBanks g;
            fp.buildGroove(g);
            p.setGrooveFromUi(g);
        }
    }

    bool saveUser(const juce::String& name) {
        auto dir = userDir();
        dir.createDirectory();
        juce::MemoryBlock mb;
        p.getStateInformation(mb);
        return dir.getChildFile(legalName(name) + ".marupreset")
                  .replaceWithData(mb.getData(), mb.getSize());
    }

    bool loadUser(const juce::File& f) {
        juce::MemoryBlock mb;
        if (!f.loadFileAsData(mb)) return false;
        p.setStateInformation(mb.getData(), (int) mb.getSize());
        return true;
    }

    juce::Array<juce::File> userPresets() const {
        return userDir().findChildFiles(juce::File::findFiles, false, "*.marupreset");
    }

private:
    static juce::String legalName(const juce::String& s) {
        return juce::File::createLegalFileName(s.isEmpty() ? "Untitled" : s);
    }
    MaruMoriProcessor& p;
};

} // namespace maru::presets
