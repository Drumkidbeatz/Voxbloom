#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vb::ui
{
    namespace colours
    {
        inline const juce::Colour background { 0xff0d0a13 };
        inline const juce::Colour panel      { 0xff17121f };
        inline const juce::Colour panelEdge  { 0xff2a2236 };
        inline const juce::Colour track      { 0xff2d2539 };
        inline const juce::Colour accent     { 0xffa970ff };
        inline const juce::Colour accent2    { 0xffff5fa2 };
        inline const juce::Colour text       { 0xffede7f6 };
        inline const juce::Colour textDim    { 0xff8d82a3 };
    }

    class VoxLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        VoxLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                                   bool highlighted, bool down) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };
}
