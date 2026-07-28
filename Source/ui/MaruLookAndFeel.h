#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace maru::ui {

// Maru Mori palette — Moog Grandmother energy: a dark chassis with each
// functional section living on its own pastel color block. Per Drew:
// "somewhat colorful, like the Moog Mother."
namespace pal {
inline const juce::Colour chassis   { 0xff14171e }; // window ground
inline const juce::Colour panel     { 0xff1c2027 }; // section body fallback
inline const juce::Colour hairline  { 0x14ffffff };
inline const juce::Colour text      { 0xffe8e4da }; // warm off-white
inline const juce::Colour textDim   { 0xff8b8fa0 };

// part / section identities
inline const juce::Colour bass      { 0xffe8907b }; // coral
inline const juce::Colour acid      { 0xffa8c64e }; // acid green
inline const juce::Colour drums     { 0xffe8b84b }; // amber
inline const juce::Colour pad       { 0xff7bb8e0 }; // sky blue
inline const juce::Colour fx        { 0xffa98fd1 }; // lavender
inline const juce::Colour mixer     { 0xffd8d3c8 }; // cream
inline const juce::Colour master    { 0xffd8d3c8 };

// each section paints a soft tinted block of its accent over the panel
inline juce::Colour block(juce::Colour accent) {
    return panel.interpolatedWith(accent, 0.10f);
}
} // namespace pal

class MaruLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MaruLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, pal::chassis);
        setColour(juce::Label::textColourId, pal::text);
        setColour(juce::Slider::textBoxTextColourId, pal::text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId, pal::panel);
        setColour(juce::ComboBox::textColourId, pal::text);
        setColour(juce::ComboBox::outlineColourId, pal::hairline);
        setColour(juce::ComboBox::arrowColourId, pal::mixer);
        setColour(juce::PopupMenu::backgroundColourId, pal::panel);
        setColour(juce::PopupMenu::textColourId, pal::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  pal::panel.brighter(0.2f));
        setColour(juce::TextButton::buttonColourId, pal::panel);
        setColour(juce::TextButton::textColourOffId, pal::text);
        setColour(juce::TextEditor::backgroundColourId, pal::panel);
        setColour(juce::TextEditor::textColourId, pal::text);
        setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                  juce::Colour(0xffe8e4da));
        setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                  juce::Colour(0xff14171e));
        setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                  pal::bass.withAlpha(0.8f));
        setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                  pal::bass.withAlpha(0.3f));
    }

    // The per-widget accent rides in on rotarySliderFillColourId, set by the
    // section builder — one LookAndFeel, many colors.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override {
        const auto bounds = juce::Rectangle<int>(x, y, width, height)
                                .toFloat().reduced(4.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle
            + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const float lw = juce::jmax(2.0f, radius * 0.14f);

        // cap
        g.setColour(pal::chassis.brighter(0.12f));
        g.fillEllipse(centre.x - radius * 0.72f, centre.y - radius * 0.72f,
                      radius * 1.44f, radius * 1.44f);

        // track + value arc
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - lw * 0.5f, radius - lw * 0.5f,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(pal::hairline);
        g.strokePath(track, juce::PathStrokeType(lw, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, radius - lw * 0.5f, radius - lw * 0.5f,
                            0.0f, rotaryStartAngle, angle, true);
        g.setColour(accent);
        g.strokePath(value, juce::PathStrokeType(lw, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // pointer
        juce::Path pointer;
        pointer.addRoundedRectangle(-lw * 0.4f, -radius * 0.66f,
                                    lw * 0.8f, radius * 0.34f, lw * 0.3f);
        g.setColour(pal::text);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle)
                                .translated(centre.x, centre.y));
        g.setColour(accent.withAlpha(0.9f));
        g.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                          bool highlighted, bool) override {
        auto r = b.getLocalBounds().toFloat().reduced(2.0f);
        const auto accent = b.findColour(juce::TextButton::buttonOnColourId);
        const bool on = b.getToggleState();

        auto pill = r.removeFromLeft(juce::jmin(r.getWidth(), 34.0f))
                     .withSizeKeepingCentre(30.0f, 16.0f);
        g.setColour(on ? accent.withAlpha(0.9f)
                       : pal::chassis.brighter(highlighted ? 0.25f : 0.12f));
        g.fillRoundedRectangle(pill, 8.0f);
        g.setColour(pal::hairline);
        g.drawRoundedRectangle(pill, 8.0f, 1.0f);
        const float kx = on ? pill.getRight() - 13.0f : pill.getX() + 3.0f;
        g.setColour(on ? pal::chassis : pal::textDim);
        g.fillEllipse(kx, pill.getY() + 3.0f, 10.0f, 10.0f);

        g.setColour(pal::text);
        g.setFont(12.0f);
        g.drawText(b.getButtonText(), b.getLocalBounds().withTrimmedLeft(38),
                   juce::Justification::centredLeft);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override {
        const auto r = juce::Rectangle<int>(0, 0, width, height).toFloat();
        g.setColour(pal::chassis.brighter(0.10f));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(pal::hairline);
        g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
        arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.5f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(arrow);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override {
        return juce::Font(juce::FontOptions(13.0f));
    }
    juce::Font getPopupMenuFont() override {
        return juce::Font(juce::FontOptions(13.0f));
    }
    juce::Font getLabelFont(juce::Label&) override {
        return juce::Font(juce::FontOptions(11.0f));
    }
};

} // namespace maru::ui
