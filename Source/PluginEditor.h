#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/VoxLookAndFeel.h"

/** Scrolling picture of the last four seconds: the vocal as a line, the effect
    as a glowing area, with beat lines from the host tempo. */
class ScopeView final : public juce::Component
{
public:
    explicit ScopeView (VoxBloomProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    VoxBloomProcessor& processor;
};

class VoxBloomEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit VoxBloomEditor (VoxBloomProcessor&);
    ~VoxBloomEditor() override;

    void resized() override;

    static constexpr int baseWidth = 1120, baseHeight = 800;

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

    /** Holds every control at the design size; the editor scales it as a whole. */
    struct Content final : public juce::Component
    {
        explicit Content (VoxBloomEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        void resized() override { owner.layoutContent(); }
        VoxBloomEditor& owner;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& caption, const juce::String& tooltip);
    Knob* knob (const juce::String& paramId);
    void addToggle (juce::ToggleButton&, const juce::String& paramId, const juce::String& tooltip);
    void addChoice (juce::ComboBox&, const juce::String& paramId, const juce::StringArray& items, const juce::String& tooltip);
    void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<const juce::String*> ids);

    void paintContent (juce::Graphics&);
    void layoutContent();

    void timerCallback() override;
    void rebuildPresetMenu();
    void presetSelected();
    void stepPreset (int delta);
    void showSaveDialog();
    void setThrowHeld (bool held);
    void setZoom (float scale);

    VoxBloomProcessor& processor;
    vb::ui::VoxLookAndFeel lnf;
    Content content { *this };
    ScopeView scope { processor };
    juce::TooltipWindow tooltips { this, 700 };

    std::vector<std::pair<juce::String, std::unique_ptr<Knob>>> knobs;

    juce::ToggleButton syncButton { "SYNC" }, freezeButton { "FREEZE" }, throwModeButton { "THROW MODE" };
    juce::TextButton throwButton { "THROW" };
    juce::ComboBox modeBox, pitchBox, sweepModeBox;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    bool throwHeld = false, constructed = false;

    juce::ComboBox presetBox, zoomBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    juce::TextButton slotA { "A" }, slotB { "B" }, copyButton { "A > B" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    std::array<Section, 9> sections;
    juce::Rectangle<int> duckMeterBounds, titleBounds, bpmBounds;
    float duckDisplay = 1.0f;
    double bpmDisplay = 120.0;

    static constexpr int userIdOffset = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBloomEditor)
};
