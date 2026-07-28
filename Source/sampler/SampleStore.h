#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "../engine/dsp/SampleBuffer.h"

// Ported verbatim from abiogenesis/Source/sampler/SampleStore.h (the proven
// RT-safe handoff — do NOT invent a new scheme).

namespace maru {

// RT-safe sample handoff: the message thread loads files and publishes a raw
// pointer with a generation stamp; the audio thread reads two atomics per
// block and nothing is freed until the audio thread has observed a NEWER
// generation, so a snapshotted pointer can never dangle.
class SampleStore {
public:
    SampleStore() { formats.registerBasicFormats(); } // WAV/AIFF/FLAC

    // message thread only
    bool loadFile(const juce::File& file) {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples < 8)
            return false;

        auto buf = std::make_shared<SampleBuffer>();
        const int frames = (int) juce::jmin<juce::int64>(reader->lengthInSamples,
                                                         48000 * 30); // 30 s cap (drums)
        juce::AudioBuffer<float> tmp((int) reader->numChannels, frames);
        reader->read(&tmp, 0, frames, 0, true, true);

        buf->left.resize((size_t) frames);
        buf->right.resize((size_t) frames);
        const float* l = tmp.getReadPointer(0);
        const float* r = tmp.getNumChannels() > 1 ? tmp.getReadPointer(1) : l;
        for (int i = 0; i < frames; ++i) {
            buf->left[(size_t) i] = l[i];
            buf->right[(size_t) i] = r[i];
        }
        buf->sourceRate = reader->sampleRate;
        buf->numFrames = frames;
        buf->generation = ++generationCounter;

        if (current != nullptr)
            retired.push_back(std::move(current));
        current = std::move(buf);
        publishedGen.store(generationCounter, std::memory_order_release);
        activePtr.store(current.get(), std::memory_order_release);
        loadedPath = file.getFullPathName();
        return true;
    }

    void clear() {
        if (current != nullptr)
            retired.push_back(std::move(current));
        publishedGen.store(++generationCounter, std::memory_order_release);
        activePtr.store(nullptr, std::memory_order_release);
        loadedPath.clear();
    }

    // audio thread: a handful of atomics, nothing else
    const SampleBuffer* acquire() const { return activePtr.load(std::memory_order_acquire); }
    void markSeen(const SampleBuffer* acquired) {
        audioSeen.store(acquired != nullptr ? acquired->generation
                                            : publishedGen.load(std::memory_order_acquire),
                        std::memory_order_release);
    }

    // message-thread timer: free retired buffers the audio thread has moved past
    void collectGarbage() {
        const uint64_t seen = audioSeen.load(std::memory_order_acquire);
        for (size_t i = retired.size(); i-- > 0;)
            if (retired[i]->generation < seen)
                retired.erase(retired.begin() + (long) i);
    }

    juce::String getLoadedPath() const { return loadedPath; }

private:
    juce::AudioFormatManager formats;
    std::shared_ptr<SampleBuffer> current;
    std::vector<std::shared_ptr<SampleBuffer>> retired;
    std::atomic<const SampleBuffer*> activePtr { nullptr };
    std::atomic<uint64_t> audioSeen { 0 };
    std::atomic<uint64_t> publishedGen { 0 };
    uint64_t generationCounter = 0;
    juce::String loadedPath;
};

// 8 independent proven stores + one shared GC timer, wrapped for the drum
// pads. Per-slot markSeen must run every block (even for silent pads) or GC
// stalls on that slot.
class DrumSampleStores : private juce::Timer {
public:
    static constexpr int kSlots = 8;

    DrumSampleStores() { startTimer(1000); }
    ~DrumSampleStores() override { stopTimer(); }

    bool load(int slot, const juce::File& f) {
        return valid(slot) ? stores[slot].loadFile(f) : false;
    }
    void clearSlot(int slot) { if (valid(slot)) stores[slot].clear(); }
    juce::String path(int slot) const {
        return valid(slot) ? stores[slot].getLoadedPath() : juce::String();
    }

    // audio thread
    const SampleBuffer* acquire(int slot) { return stores[slot].acquire(); }
    void markSeen(int slot, const SampleBuffer* b) { stores[slot].markSeen(b); }

private:
    static bool valid(int s) { return s >= 0 && s < kSlots; }
    void timerCallback() override {
        for (auto& s : stores) s.collectGarbage();
    }
    SampleStore stores[kSlots];
};

} // namespace maru
