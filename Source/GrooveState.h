#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Patterns.h"

// GROOVE side-block: all four parts' pattern data serialized as packed strings
// on one versioned child of the APVTS state tree (fleet law: patterns are
// state, never automatable parameters). Packed strings keep the XML a few KB.
//
// <GROOVE ver="1" bass="n,o,g,a,s,gl,st,cv;..." acid="n,o,g,a,s;..."
//         d0="1000..." da0="0000..." ... d7 da7
//         pad="g,c,t;..." c0="57,60,64,67" c1..c3/>

namespace maru::state {

inline constexpr const char* kGrooveType = "GROOVE";
inline constexpr int kGrooveVersion = 1;

// --- packing -----------------------------------------------------------------

inline juce::String packBass(const BassPattern& p) {
    juce::String out;
    for (const auto& s : p.steps)
        out << s.note << ',' << s.oct << ',' << (s.gate ? 1 : 0) << ','
            << (s.accent ? 1 : 0) << ',' << (s.slide ? 1 : 0) << ','
            << s.gateLen << ',' << s.slideT << ','
            << juce::String(s.cvOct, 3) << ';';
    return out;
}

inline juce::String packAcid(const AcidPattern& p) {
    juce::String out;
    for (const auto& s : p.steps)
        out << s.note << ',' << s.oct << ',' << (s.gate ? 1 : 0) << ','
            << (s.accent ? 1 : 0) << ',' << (s.slide ? 1 : 0) << ';';
    return out;
}

inline juce::String packLane(const DrumGrid& g, int lane, bool accent) {
    juce::String out;
    for (int i = 0; i < 16; ++i) {
        const auto& c = g.cells[lane][i];
        out << ((accent ? c.accent : c.on) ? '1' : '0');
    }
    return out;
}

inline juce::String packPad(const PadPattern& p) {
    juce::String out;
    for (const auto& s : p.steps)
        out << (s.gate ? 1 : 0) << ',' << s.chord << ',' << (s.tie ? 1 : 0) << ';';
    return out;
}

inline juce::String packChord(const PadChord& c) {
    juce::String out;
    for (int i = 0; i < c.count && i < 4; ++i) {
        if (i) out << ',';
        out << c.notes[i];
    }
    return out;
}

// --- unpacking (tolerant: malformed fields keep defaults) --------------------

inline void unpackBass(const juce::String& s, BassPattern& p) {
    auto rows = juce::StringArray::fromTokens(s, ";", "");
    for (int i = 0; i < 16 && i < rows.size(); ++i) {
        auto f = juce::StringArray::fromTokens(rows[i], ",", "");
        if (f.size() < 8) continue;
        auto& st = p.steps[i];
        st.note = f[0].getIntValue();  st.oct = f[1].getIntValue();
        st.gate = f[2].getIntValue() != 0;
        st.accent = f[3].getIntValue() != 0;
        st.slide = f[4].getIntValue() != 0;
        st.gateLen = f[5].getIntValue();
        st.slideT = f[6].getIntValue();
        st.cvOct = f[7].getFloatValue();
    }
}

inline void unpackAcid(const juce::String& s, AcidPattern& p) {
    auto rows = juce::StringArray::fromTokens(s, ";", "");
    for (int i = 0; i < 16 && i < rows.size(); ++i) {
        auto f = juce::StringArray::fromTokens(rows[i], ",", "");
        if (f.size() < 5) continue;
        auto& st = p.steps[i];
        st.note = f[0].getIntValue();  st.oct = f[1].getIntValue();
        st.gate = f[2].getIntValue() != 0;
        st.accent = f[3].getIntValue() != 0;
        st.slide = f[4].getIntValue() != 0;
    }
}

inline void unpackLane(const juce::String& s, DrumGrid& g, int lane, bool accent) {
    for (int i = 0; i < 16 && i < s.length(); ++i) {
        const bool v = s[i] == '1';
        if (accent) g.cells[lane][i].accent = v;
        else        g.cells[lane][i].on = v;
    }
}

inline void unpackPad(const juce::String& s, PadPattern& p) {
    auto rows = juce::StringArray::fromTokens(s, ";", "");
    for (int i = 0; i < 16 && i < rows.size(); ++i) {
        auto f = juce::StringArray::fromTokens(rows[i], ",", "");
        if (f.size() < 3) continue;
        auto& st = p.steps[i];
        st.gate = f[0].getIntValue() != 0;
        st.chord = f[1].getIntValue();
        st.tie = f[2].getIntValue() != 0;
    }
}

inline void unpackChord(const juce::String& s, PadChord& c) {
    auto f = juce::StringArray::fromTokens(s, ",", "");
    if (f.isEmpty()) return;
    c.count = 0;
    for (int i = 0; i < 4 && i < f.size(); ++i)
        c.notes[c.count++] = f[i].getIntValue();
}

// --- tree <-> struct ---------------------------------------------------------

inline void writeGroove(juce::ValueTree& parent, const GroovePatterns& g) {
    auto t = parent.getOrCreateChildWithName(kGrooveType, nullptr);
    t.setProperty("ver", kGrooveVersion, nullptr);
    t.setProperty("bass", packBass(g.bass), nullptr);
    t.setProperty("acid", packAcid(g.acid), nullptr);
    for (int l = 0; l < 8; ++l) {
        t.setProperty("d" + juce::String(l), packLane(g.drums, l, false), nullptr);
        t.setProperty("da" + juce::String(l), packLane(g.drums, l, true), nullptr);
    }
    t.setProperty("pad", packPad(g.pad), nullptr);
    for (int c = 0; c < 4; ++c)
        t.setProperty("c" + juce::String(c), packChord(g.pad.chords[c]), nullptr);
}

inline GroovePatterns readGroove(const juce::ValueTree& parent) {
    GroovePatterns g;
    auto t = parent.getChildWithName(kGrooveType);
    if (!t.isValid())
        return g;
    unpackBass(t.getProperty("bass").toString(), g.bass);
    unpackAcid(t.getProperty("acid").toString(), g.acid);
    for (int l = 0; l < 8; ++l) {
        unpackLane(t.getProperty("d" + juce::String(l)).toString(), g.drums, l, false);
        unpackLane(t.getProperty("da" + juce::String(l)).toString(), g.drums, l, true);
    }
    unpackPad(t.getProperty("pad").toString(), g.pad);
    for (int c = 0; c < 4; ++c)
        unpackChord(t.getProperty("c" + juce::String(c)).toString(), g.pad.chords[c]);
    return g;
}

} // namespace maru::state
