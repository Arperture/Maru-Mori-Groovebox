#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GrooveState.h"

MaruMoriProcessor::MaruMoriProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", maru::params::createParameterLayout()),
      paramRefs(apvts) {
    writeGrooveToState(); // default patterns -> GROOVE side-block
}

void MaruMoriProcessor::loadGrooveFromState() {
    if (!apvts.state.getChildWithName(maru::state::kGrooveType).isValid()) {
        writeGrooveToState(); // old/absent state: keep defaults, re-create
        return;
    }
    auto g = maru::state::readGroove(apvts.state);
    const juce::SpinLock::ScopedLockType lock(grooveLock);
    groove = g;
}

void MaruMoriProcessor::writeGrooveToState() {
    maru::GrooveBanks g;
    {
        const juce::SpinLock::ScopedLockType lock(grooveLock);
        g = groove;
    }
    maru::state::writeGroove(apvts.state, g);
}

void MaruMoriProcessor::setGrooveFromUi(const maru::GrooveBanks& g) {
    {
        const juce::SpinLock::ScopedLockType lock(grooveLock);
        groove = g;
    }
    writeGrooveToState();
}

maru::GrooveBanks MaruMoriProcessor::getGroove() const {
    const juce::SpinLock::ScopedLockType lock(grooveLock);
    return groove;
}

void MaruMoriProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);
}

bool MaruMoriProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MaruMoriProcessor::handleMidiEvent(const juce::MidiMessage& m) {
    if (m.isNoteOn())
        engine.noteOn(m.getChannel(), m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff(m.getChannel(), m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void MaruMoriProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer(midi, 0, numSamples, true);

    engine.setParams(paramRefs.snapshot());

    {
        // non-blocking: if the message thread is mid-edit, keep last patterns
        const juce::SpinLock::ScopedTryLockType tryLock(grooveLock);
        if (tryLock.isLocked())
            engine.setPatterns(groove);
    }

    {
        const maru::SampleBuffer* bufs[8];
        for (int s = 0; s < 8; ++s) {
            bufs[s] = drumStores.acquire(s);
            drumStores.markSeen(s, bufs[s]); // every block, even silent pads
        }
        engine.setDrumSamples(bufs);
    }

    maru::TransportInfo transport;
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            if (auto bpm = pos->getBpm()) transport.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) transport.ppq = *ppq;
            transport.playing = pos->getIsPlaying();
        }
    }

    float* left  = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    // render in segments split at MIDI event positions (sample-accurate)
    int pos = 0;
    for (const auto metadata : midi) {
        const int eventPos = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (eventPos > pos) {
            engine.process(left + pos, right + pos, eventPos - pos, transport);
            pos = eventPos;
        }
        handleMidiEvent(metadata.getMessage());
    }
    if (pos < numSamples)
        engine.process(left + pos, right + pos, numSamples - pos, transport);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

void MaruMoriProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    auto kit = state.getOrCreateChildWithName("DRUMKIT", nullptr);
    for (int s = 0; s < 8; ++s)
        kit.setProperty("path" + juce::String(s), drumStores.path(s), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void MaruMoriProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        apvts.replaceState(state);
        loadGrooveFromState();
        auto kit = state.getChildWithName("DRUMKIT");
        if (kit.isValid()) {
            juce::StringArray paths;
            for (int s = 0; s < 8; ++s)
                paths.add(kit.getProperty("path" + juce::String(s)).toString());
            juce::MessageManager::callAsync([this, paths] {
                for (int s = 0; s < 8; ++s) {
                    if (paths[s].isEmpty()) { drumStores.clearSlot(s); continue; }
                    juce::File f(paths[s]);
                    if (f.existsAsFile())
                        drumStores.load(s, f); // missing file: keep DefaultKit
                }
            });
        }
    }
}

juce::AudioProcessorEditor* MaruMoriProcessor::createEditor() {
    return new MaruMoriEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MaruMoriProcessor();
}
