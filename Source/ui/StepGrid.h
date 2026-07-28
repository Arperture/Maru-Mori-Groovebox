#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "MaruLookAndFeel.h"

namespace maru::ui {

// Pattern editor for all four parts. One tab per part (its own color), a bank
// selector (edits what the seq*Bank param has selected — you edit what you
// hear), and a click/drag cell grid:
//   bass/acid: NOTE (drag) / GATE / ACC / SLIDE rows (+ bass: LEN, SLIDE-T, CV)
//   drums:     8 lanes x 16 cells, click = toggle, alt/right-click = accent
//   pad:       step click cycles off -> chord -> tie; chord row drags 1-4;
//              chord editor drags the 4x4 note table
class StepGrid : public juce::Component, private juce::Timer {
public:
    explicit StepGrid(MaruMoriProcessor& proc) : p(proc) {
        groove = p.getGroove();
        startTimer(300); // follow external bank/state changes
        for (int i = 0; i < 4; ++i) {
            auto* b = tabs.add(new juce::TextButton(kTabName[i]));
            b->setClickingTogglesState(false);
            b->onClick = [this, i] { tab = i; repaint(); };
            addAndMakeVisible(b);
        }
        bankBox.addItemList({ "BANK A", "BANK B", "BANK C", "BANK D" }, 1);
        addAndMakeVisible(bankBox);
        bankBox.onChange = [this] {
            if (auto* prm = p.apvts.getParameter(bankParamId(tab)))
                prm->setValueNotifyingHost(
                    prm->convertTo0to1((float) bankBox.getSelectedItemIndex()));
            repaint();
        };
    }

    void resized() override {
        auto r = getLocalBounds().reduced(6);
        auto head = r.removeFromTop(24);
        for (auto* b : tabs)
            b->setBounds(head.removeFromLeft(70).reduced(2, 0));
        bankBox.setBounds(head.removeFromLeft(100).reduced(2, 0));
        gridArea = r.reduced(0, 4);
    }

    void paint(juce::Graphics& g) override {
        g.setColour(pal::block(accent()));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        g.setColour(pal::hairline);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);

        for (int i = 0; i < tabs.size(); ++i) {
            tabs[i]->setColour(juce::TextButton::buttonColourId,
                tab == i ? kTabColour[i].withAlpha(0.85f) : pal::panel);
            tabs[i]->setColour(juce::TextButton::textColourOffId,
                tab == i ? pal::chassis : pal::textDim);
        }

        switch (tab) {
            case 0: paintMono(g, true);  break;
            case 1: paintMono(g, false); break;
            case 2: paintDrums(g);       break;
            case 3: paintPad(g);         break;
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragRow = dragCol = -1;
        if (!gridArea.contains(e.getPosition())) return;
        switch (tab) {
            case 0: monoDown(e, true);  break;
            case 1: monoDown(e, false); break;
            case 2: drumsDown(e);       break;
            case 3: padDown(e);         break;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragRow < 0) return;
        const int dy = (e.getPosition().y - dragStartY) / 8; // 8 px per unit
        applyDrag(dy);
    }

    void mouseUp(const juce::MouseEvent&) override { dragRow = dragCol = -1; }

private:
    static constexpr const char* kTabName[4] = { "BASS", "ACID", "DRUMS", "PAD" };
    inline static const juce::Colour kTabColour[4] = {
        pal::bass, pal::acid, pal::drums, pal::pad };

    MaruMoriProcessor& p;
    GrooveBanks groove;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::ComboBox bankBox;
    juce::Rectangle<int> gridArea;
    int tab = 0;
    int dragRow = -1, dragCol = -1, dragStartY = 0, dragStartVal = 0;
    float dragStartValF = 0.0f;

    static const char* bankParamId(int t) {
        static const char* ids[4] = { "seqBassBank", "seqAcidBank",
                                      "seqDrumBank", "seqPadBank" };
        return ids[t];
    }

    juce::Colour accent() const { return kTabColour[tab]; }

    int bank() const {
        if (auto* v = p.apvts.getRawParameterValue(bankParamId(tab)))
            return juce::jlimit(0, kNumBanks - 1, (int) v->load());
        return 0;
    }

    void timerCallback() override {
        bankBox.setSelectedItemIndex(bank(), juce::dontSendNotification);
    }

    void push() { p.setGrooveFromUi(groove); repaint(); }

    static juce::String noteName(int n) {
        static const char* names[12] = { "C","C#","D","D#","E","F",
                                         "F#","G","G#","A","A#","B" };
        return juce::String(names[((n % 12) + 12) % 12]) + juce::String(n / 12 - 1);
    }

    // ---- geometry -----------------------------------------------------------
    juce::Rectangle<float> cell(int col, int row, int nRows) const {
        const float cw = (float) gridArea.getWidth() / 16.0f;
        const float ch = (float) gridArea.getHeight() / (float) nRows;
        return { (float) gridArea.getX() + col * cw,
                 (float) gridArea.getY() + row * ch, cw, ch };
    }
    int hitCol(const juce::MouseEvent& e) const {
        return juce::jlimit(0, 15,
            (int) ((e.position.x - (float) gridArea.getX())
                   / ((float) gridArea.getWidth() / 16.0f)));
    }
    int hitRow(const juce::MouseEvent& e, int nRows) const {
        return juce::jlimit(0, nRows - 1,
            (int) ((e.position.y - (float) gridArea.getY())
                   / ((float) gridArea.getHeight() / (float) nRows)));
    }

    // ---- bass / acid --------------------------------------------------------
    int monoRows(bool isBass) const { return isBass ? 7 : 4; }

    void paintMono(juce::Graphics& g, bool isBass) {
        const int rows = monoRows(isBass);
        const int b = bank();
        static const char* rowName[7] = { "NOTE", "GATE", "ACC", "SLIDE",
                                          "LEN", "TIME", "CV" };
        for (int c = 0; c < 16; ++c) {
            const bool beat = (c % 4) == 0;
            for (int r = 0; r < rows; ++r) {
                auto cl = cell(c, r, rows).reduced(1.5f);
                const int note   = isBass ? groove.bass[b].steps[c].note
                                          : groove.acid[b].steps[c].note;
                const bool gate  = isBass ? groove.bass[b].steps[c].gate
                                          : groove.acid[b].steps[c].gate;
                const bool acc   = isBass ? groove.bass[b].steps[c].accent
                                          : groove.acid[b].steps[c].accent;
                const bool slide = isBass ? groove.bass[b].steps[c].slide
                                          : groove.acid[b].steps[c].slide;
                juce::Colour base = pal::chassis.brighter(beat ? 0.16f : 0.08f);
                g.setColour(base);
                g.fillRoundedRectangle(cl, 3.0f);
                g.setColour(pal::text);
                g.setFont(10.0f);
                switch (r) {
                    case 0:
                        if (gate) {
                            g.setColour(accent());
                            g.drawText(noteName(note), cl, juce::Justification::centred);
                        } else {
                            g.setColour(pal::textDim);
                            g.drawText("-", cl, juce::Justification::centred);
                        }
                        break;
                    case 1: fillLed(g, cl, gate, accent()); break;
                    case 2: fillLed(g, cl, acc, pal::drums); break;
                    case 3: fillLed(g, cl, slide, pal::pad); break;
                    case 4: { // bass gate length 1-4 blocks
                        const int v = groove.bass[b].steps[c].gateLen;
                        barBlocks(g, cl, v + 1, 4, accent());
                        break;
                    }
                    case 5: {
                        const int v = groove.bass[b].steps[c].slideT;
                        barBlocks(g, cl, v + 1, 4, pal::pad);
                        break;
                    }
                    case 6: { // filter CV bipolar bar
                        const float v = groove.bass[b].steps[c].cvOct; // -4..4
                        const float mid = cl.getCentreY();
                        g.setColour(pal::hairline);
                        g.drawHorizontalLine((int) mid, cl.getX(), cl.getRight());
                        g.setColour(pal::fx);
                        const float h = (v / 4.0f) * (cl.getHeight() * 0.5f);
                        g.fillRect(cl.getX() + 2.0f, h > 0 ? mid - h : mid,
                                   cl.getWidth() - 4.0f, std::fabs(h));
                        break;
                    }
                }
            }
        }
        g.setColour(pal::textDim);
        g.setFont(9.0f);
        for (int r = 0; r < rows; ++r) {
            auto cl = cell(0, r, rows);
            g.drawText(rowName[r], (int) cl.getX() - 0, (int) cl.getY(),
                       36, (int) cl.getHeight(), juce::Justification::centredLeft);
        }
    }

    void monoDown(const juce::MouseEvent& e, bool isBass) {
        const int rows = monoRows(isBass);
        const int c = hitCol(e), r = hitRow(e, rows);
        const int b = bank();
        dragCol = c; dragRow = r; dragStartY = e.getPosition().y;
        if (isBass) {
            auto& s = groove.bass[b].steps[c];
            switch (r) {
                case 0: dragStartVal = s.note; return;
                case 1: s.gate = !s.gate; break;
                case 2: s.accent = !s.accent; break;
                case 3: s.slide = !s.slide; break;
                case 4: s.gateLen = (s.gateLen + 1) & 3; break;
                case 5: s.slideT = (s.slideT + 1) & 3; break;
                case 6: dragStartValF = s.cvOct; return;
            }
        } else {
            auto& s = groove.acid[b].steps[c];
            switch (r) {
                case 0: dragStartVal = s.note; return;
                case 1: s.gate = !s.gate; break;
                case 2: s.accent = !s.accent; break;
                case 3: s.slide = !s.slide; break;
            }
        }
        push();
    }

    void applyDrag(int dy) {
        const int b = bank();
        if (tab == 0) {
            auto& s = groove.bass[b].steps[dragCol];
            if (dragRow == 0)
                s.note = juce::jlimit(24, 72, dragStartVal - dy);
            else if (dragRow == 6)
                s.cvOct = juce::jlimit(-4.0f, 4.0f, dragStartValF - (float) dy * 0.25f);
            else return;
            push();
        } else if (tab == 1 && dragRow == 0) {
            auto& s = groove.acid[b].steps[dragCol];
            s.note = juce::jlimit(24, 72, dragStartVal - dy);
            push();
        } else if (tab == 3 && dragRow >= 2) {
            chordDrag(dy);
        }
    }

    // ---- drums --------------------------------------------------------------
    void paintDrums(juce::Graphics& g) {
        const int b = bank();
        static const char* lane[8] = { "KICK", "SNARE", "CLAP", "CH",
                                       "OH", "TOM", "RIM", "SHKR" };
        for (int l = 0; l < 8; ++l) {
            for (int c = 0; c < 16; ++c) {
                auto cl = cell(c, l, 8).reduced(1.5f);
                const auto& cellData = groove.drums[b].cells[l][c];
                const bool beat = (c % 4) == 0;
                g.setColour(cellData.on
                    ? (cellData.accent ? pal::drums : pal::drums.withAlpha(0.55f))
                    : pal::chassis.brighter(beat ? 0.16f : 0.08f));
                g.fillRoundedRectangle(cl, 3.0f);
            }
            auto first = cell(0, l, 8);
            g.setColour(pal::textDim);
            g.setFont(9.0f);
            g.drawText(lane[l], (int) first.getX(), (int) first.getY(),
                       34, (int) first.getHeight(), juce::Justification::centredLeft);
        }
    }

    void drumsDown(const juce::MouseEvent& e) {
        const int c = hitCol(e), l = hitRow(e, 8);
        auto& cellData = groove.drums[bank()].cells[l][c];
        if (e.mods.isRightButtonDown() || e.mods.isAltDown()) {
            cellData.accent = !cellData.accent;
            if (cellData.accent) cellData.on = true;
        } else {
            cellData.on = !cellData.on;
            if (!cellData.on) cellData.accent = false;
        }
        push();
    }

    // ---- pad ----------------------------------------------------------------
    void paintPad(juce::Graphics& g) {
        const int b = bank();
        const auto& pat = groove.pad[b];
        // rows: 0 step state, 1 chord select, 2..5 chord editor (4 chords)
        for (int c = 0; c < 16; ++c) {
            auto cl = cell(c, 0, 6).reduced(1.5f);
            const auto& s = pat.steps[c];
            g.setColour(s.tie ? pal::pad.withAlpha(0.35f)
                       : s.gate ? pal::pad
                       : pal::chassis.brighter((c % 4) == 0 ? 0.16f : 0.08f));
            g.fillRoundedRectangle(cl, 3.0f);
            if (s.gate) {
                g.setColour(pal::chassis);
                g.setFont(10.0f);
                g.drawText(juce::String(s.chord + 1), cl, juce::Justification::centred);
            } else if (s.tie) {
                g.setColour(pal::text);
                g.drawText("~", cl, juce::Justification::centred);
            }
        }
        // chord editor: 4 chords x 4 notes on rows 2..5
        for (int ch = 0; ch < 4; ++ch) {
            for (int n = 0; n < 4; ++n) {
                auto cl = cell(ch * 4 + n, 2 + ch, 6).reduced(1.5f);
                g.setColour(pal::chassis.brighter(n == 0 ? 0.18f : 0.10f));
                g.fillRoundedRectangle(cl, 3.0f);
                g.setColour(n < pat.chords[ch].count ? pal::pad : pal::textDim);
                g.setFont(10.0f);
                g.drawText(noteName(pat.chords[ch].notes[n]), cl,
                           juce::Justification::centred);
            }
            auto cl = cell(ch * 4, 2 + ch, 6);
            g.setColour(pal::textDim);
            g.setFont(9.0f);
            g.drawText("CHD " + juce::String(ch + 1),
                       (int) cl.getX(), (int) cl.getY() - 1, 40, 10,
                       juce::Justification::topLeft);
        }
        g.setColour(pal::textDim);
        g.setFont(9.0f);
        auto r0 = cell(0, 0, 6);
        g.drawText("STEP: click = chord, again = tie, again = off. Drag chord tables.",
                   (int) r0.getX(), (int) r0.getBottom() + 2, gridArea.getWidth(), 12,
                   juce::Justification::centredLeft);
    }

    void padDown(const juce::MouseEvent& e) {
        const int b = bank();
        const int c = hitCol(e), r = hitRow(e, 6);
        auto& pat = groove.pad[b];
        if (r == 0) {
            auto& s = pat.steps[c];
            if (!s.gate && !s.tie) { s.gate = true; s.tie = false; }
            else if (s.gate && e.mods.isRightButtonDown()) {
                s.chord = (s.chord + 1) & 3; // right-click: cycle chord
            }
            else if (s.gate) { s.gate = false; s.tie = true; }
            else { s.tie = false; }
            push();
        } else if (r >= 2 && r <= 5) {
            dragRow = r; dragCol = c; dragStartY = e.getPosition().y;
            const int ch = r - 2, n = c % 4;
            if (c / 4 == ch)
                dragStartVal = pat.chords[ch].notes[n];
            else
                dragRow = -1;
        }
    }

    void chordDrag(int dy) {
        const int ch = dragRow - 2, n = dragCol % 4;
        if (dragCol / 4 != ch) return;
        auto& chord = groove.pad[bank()].chords[ch];
        chord.notes[n] = juce::jlimit(36, 84, dragStartVal - dy);
        if (chord.count < n + 1) chord.count = n + 1;
        push();
    }

    // ---- little draw helpers ------------------------------------------------
    static void fillLed(juce::Graphics& g, juce::Rectangle<float> cl,
                        bool on, juce::Colour col) {
        auto led = cl.withSizeKeepingCentre(juce::jmin(cl.getWidth() - 6.0f, 18.0f),
                                            juce::jmin(cl.getHeight() - 6.0f, 10.0f));
        g.setColour(on ? col : pal::chassis.brighter(0.2f));
        g.fillRoundedRectangle(led, 3.0f);
    }

    static void barBlocks(juce::Graphics& g, juce::Rectangle<float> cl,
                          int filled, int total, juce::Colour col) {
        const float w = cl.getWidth() / (float) total;
        for (int i = 0; i < total; ++i) {
            auto blk = juce::Rectangle<float>(cl.getX() + i * w, cl.getY(),
                                              w, cl.getHeight()).reduced(1.0f);
            g.setColour(i < filled ? col.withAlpha(0.8f)
                                   : pal::chassis.brighter(0.18f));
            g.fillRoundedRectangle(blk, 2.0f);
        }
    }
};

} // namespace maru::ui
