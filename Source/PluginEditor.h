#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/VoxLookAndFeel.h"

class VoxBloomEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit VoxBloomEditor (VoxBloomProcessor&);
    ~VoxBloomEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& caption);
    void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs);

    void timerCallback() override;
    void rebuildPresetMenu();
    void presetSelected();
    void stepPreset (int delta);
    void showSaveDialog();

    VoxBloomProcessor& processor;
    vb::ui::VoxLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* timeKnob = nullptr;
    Knob* noteKnob = nullptr;

    juce::ToggleButton syncButton { "SYNC" };
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    std::array<Section, 4> sections;
    juce::Rectangle<int> duckMeterBounds, titleBounds, bpmBounds;
    float duckDisplay = 1.0f;
    double bpmDisplay = 120.0;

    static constexpr int userIdOffset = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBloomEditor)
};
