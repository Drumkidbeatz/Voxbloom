#include "VoxLookAndFeel.h"

namespace vb::ui
{
VoxLookAndFeel::VoxLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, colours::background);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.4f));
    setColour (Label::textColourId, colours::text);
    setColour (ComboBox::backgroundColourId, colours::panel);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::outlineColourId, colours::panelEdge);
    setColour (ComboBox::arrowColourId, colours::accent);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, colours::text);
    setColour (PopupMenu::headerTextColourId, colours::accent);
    setColour (TextButton::buttonColourId, colours::panel);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, colours::text);
    setColour (TextEditor::backgroundColourId, colours::background);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, colours::panelEdge);
    setColour (TextEditor::focusedOutlineColourId, colours::accent);
    setColour (AlertWindow::backgroundColourId, colours::panel);
    setColour (AlertWindow::textColourId, colours::text);
    setColour (AlertWindow::outlineColourId, colours::panelEdge);
}

void VoxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto lineW  = jmax (3.0f, radius * 0.14f);
    const auto arcR   = radius - lineW * 0.5f;
    const auto enabled = slider.isEnabled();

    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (track, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));

    if (sliderPos > 0.0f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        ColourGradient grad (colours::accent, centre.x - radius, centre.y + radius,
                             colours::accent2, centre.x + radius, centre.y - radius, false);
        g.setGradientFill (grad);
        g.setOpacity (enabled ? 1.0f : 0.35f);
        g.strokePath (value, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
        g.setOpacity (1.0f);
    }

    const auto knobR = arcR - lineW * 1.3f;
    g.setColour (colours::panelEdge);
    g.fillEllipse (Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));
    g.setColour (colours::panel.brighter (0.08f));
    g.fillEllipse (Rectangle<float> (knobR * 2.0f - 3.0f, knobR * 2.0f - 3.0f).withCentre (centre));

    const auto tip  = centre.getPointOnCircumference (knobR * 0.78f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.25f, angle);
    g.setColour (enabled ? colours::text : colours::textDim);
    g.drawLine ({ base, tip }, jmax (2.0f, lineW * 0.55f));
}

void VoxLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace juce;
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = b.getToggleState();
    g.setColour (on ? colours::accent.withAlpha (0.85f) : colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? colours::accent : (highlighted ? colours::accent.withAlpha (0.6f) : colours::panelEdge));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? colours::background : colours::textDim);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (b.getButtonText(), r, Justification::centred);
}

void VoxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                           bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colours::accent.withAlpha (0.5f) : colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (highlighted ? colours::accent : colours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
}

void VoxLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    const auto r = Rectangle<int> (width, height).toFloat().reduced (1.0f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent : colours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    Path arrow;
    const auto ax = (float) width - 16.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (colours::accent);
    g.fillPath (arrow);
}

juce::Font VoxLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (14.0f);
}

juce::Font VoxLookAndFeel::getPopupMenuFont()
{
    return juce::FontOptions (14.0f);
}

juce::Label* VoxLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (juce::FontOptions (12.0f));
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}
}
