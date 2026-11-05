#include "PluginEditor.h"
#include "Params.h"

using namespace vb;
namespace col = vb::ui::colours;

VoxBloomEditor::VoxBloomEditor (VoxBloomProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    timeKnob = &addKnob (pid::delayTime, "Time");
    noteKnob = &addKnob (pid::delayNote, "Time");
    addKnob (pid::delayFeedback, "Feedback");
    addKnob (pid::delayLowCut,   "Low Cut");
    addKnob (pid::delayHighCut,  "High Cut");
    addKnob (pid::delayDrive,    "Drive");
    addKnob (pid::delayWobble,   "Wobble");
    addKnob (pid::delayLevel,    "Level");

    addKnob (pid::reverbSize,     "Size");
    addKnob (pid::reverbDecay,    "Decay");
    addKnob (pid::reverbPreDelay, "Pre-Delay");
    addKnob (pid::reverbDamp,     "Damping");
    addKnob (pid::reverbLowCut,   "Low Cut");
    addKnob (pid::reverbMod,      "Modulation");
    addKnob (pid::reverbLevel,    "Level");
    addKnob (pid::reverbSource,   "Delay > Verb");

    addKnob (pid::duckAmount,    "Amount");
    addKnob (pid::duckThreshold, "Threshold");
    addKnob (pid::duckRelease,   "Release");

    addKnob (pid::width,  "Width");
    addKnob (pid::mix,    "Mix");
    addKnob (pid::output, "Output");

    addAndMakeVisible (syncButton);
    syncButton.setTooltip ("Sync the delay time to the host tempo");
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, pid::delaySync, syncButton);

    modeBox.addItemList (delayModeNames(), 1);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, pid::delayMode, modeBox);

    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] { presetSelected(); };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    for (auto* b : { &prevButton, &nextButton, &saveButton })
        addAndMakeVisible (b);

    rebuildPresetMenu();

    setSize (980, 600);
    timerCallback();
    startTimerHz (30);
}

VoxBloomEditor::~VoxBloomEditor()
{
    stopTimer();
    if (saveDialog != nullptr)
        saveDialog->exitModalState (0);
    saveDialog.reset();
    setLookAndFeel (nullptr);
}

VoxBloomEditor::Knob& VoxBloomEditor::addKnob (const juce::String& paramId, const juce::String& caption)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
    k->label.setText (caption, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    k->label.setColour (juce::Label::textColourId, col::textDim);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k->slider);

    // Set explicitly so the value box is rebuilt with these colours.
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

//==============================================================================
void VoxBloomEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);

    juce::String lastCategory;
    const auto& factory = factoryPresets();
    for (size_t i = 0; i < factory.size(); ++i)
    {
        if (factory[i].category != lastCategory)
        {
            presetBox.addSectionHeading (factory[i].category);
            lastCategory = factory[i].category;
        }
        presetBox.addItem (factory[i].name, (int) i + 1);
    }

    userPresetFiles = UserPresets::list();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdOffset + i);
    }

    presetBox.setText (processor.getCurrentPresetName(), juce::dontSendNotification);
}

void VoxBloomEditor::presetSelected()
{
    const auto id = presetBox.getSelectedId();
    if (id <= 0)
        return;

    if (id >= userIdOffset)
    {
        const auto index = id - userIdOffset;
        if (juce::isPositiveAndBelow (index, userPresetFiles.size()))
            processor.loadUserPreset (userPresetFiles[index]);
    }
    else
    {
        processor.loadFactoryPreset (id - 1);
    }
}

void VoxBloomEditor::stepPreset (int delta)
{
    juce::Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        ids.add (presetBox.getItemId (i));

    if (ids.isEmpty())
        return;

    auto pos = ids.indexOf (presetBox.getSelectedId());
    if (pos < 0)
    {
        // Nothing selected (e.g. after loading a session): find by name.
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == processor.getCurrentPresetName())
                pos = i;
    }

    pos = pos < 0 ? 0 : (pos + delta + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[pos], juce::sendNotificationSync);
}

void VoxBloomEditor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name for your preset:",
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", processor.getCurrentPresetName());
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<VoxBloomEditor> safeThis (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis] (int result)
    {
        if (safeThis == nullptr || safeThis->saveDialog == nullptr)
            return;

        const auto name = safeThis->saveDialog->getTextEditorContents ("name").trim();
        if (result == 1 && name.isNotEmpty())
        {
            if (safeThis->processor.saveUserPreset (name))
                safeThis->rebuildPresetMenu();
            else
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle ("Could not save preset")
                                                  .withMessage ("The preset folder is not writable:\n"
                                                                + UserPresets::folder().getFullPathName())
                                                  .withButton ("OK", 0),
                                              nullptr);
        }

        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->saveDialog.reset();
        });
    }), false);
}

//==============================================================================
void VoxBloomEditor::timerCallback()
{
    const auto synced = processor.apvts.getRawParameterValue (pid::delaySync)->load() > 0.5f;
    timeKnob->slider.setVisible (! synced);
    noteKnob->slider.setVisible (synced);

    const auto name = processor.getCurrentPresetName();
    if (presetBox.getText() != name && ! presetBox.isPopupActive())
        presetBox.setText (name, juce::dontSendNotification);

    const auto duck = processor.getDuckGainForDisplay();
    const auto bpm  = processor.getCurrentBpm();
    if (std::abs (duck - duckDisplay) > 0.002f || std::abs (bpm - bpmDisplay) > 0.01)
    {
        duckDisplay = duck;
        bpmDisplay = bpm;
        repaint (duckMeterBounds.expanded (4));
        repaint (bpmBounds);
    }
}

//==============================================================================
void VoxBloomEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (col::background);

    // Title
    {
        auto r = titleBounds;
        g.setGradientFill (ColourGradient (col::accent, (float) r.getX(), 0.0f,
                                           col::accent2, (float) r.getX() + 170.0f, 0.0f, false));
        g.setFont (FontOptions (26.0f, Font::bold));
        g.drawText ("VOXBLOOM", r.removeFromTop (30), Justification::bottomLeft);
        g.setColour (col::textDim);
        g.setFont (FontOptions (12.0f));
        g.drawText ("vocal delay  +  reverb  +  ducking", r, Justification::topLeft);
    }

    g.setColour (col::textDim);
    g.setFont (FontOptions (12.0f));
    g.drawText (String (bpmDisplay, 1) + " BPM", bpmBounds, Justification::centredRight);

    for (const auto& s : sections)
    {
        const auto r = s.bounds.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        g.setColour (col::accent);
        g.setFont (FontOptions (13.0f, Font::bold));
        g.drawText (s.title, s.bounds.reduced (16, 0).removeFromTop (34), Justification::centredLeft);
    }

    // Ducking meter: how far the wet signal is currently pushed down.
    if (! duckMeterBounds.isEmpty())
    {
        auto r = duckMeterBounds.toFloat();
        g.setColour (col::textDim);
        g.setFont (FontOptions (11.0f, Font::bold));
        g.drawText ("DUCK", r.removeFromTop (16.0f), Justification::centred);

        auto bar = r.removeFromTop (r.getHeight() - 18.0f);
        bar = bar.withSizeKeepingCentre (14.0f, bar.getHeight());
        g.setColour (col::track);
        g.fillRoundedRectangle (bar, 4.0f);

        const auto reduction = jlimit (0.0f, 1.0f, 1.0f - duckDisplay);
        auto fill = bar.withHeight (bar.getHeight() * reduction);
        g.setGradientFill (ColourGradient (col::accent, 0.0f, bar.getY(), col::accent2, 0.0f, bar.getBottom(), false));
        g.fillRoundedRectangle (fill, 4.0f);

        const auto db = Decibels::gainToDecibels (jmax (0.001f, duckDisplay));
        g.setColour (col::text);
        g.setFont (FontOptions (11.0f));
        g.drawText (String (db, 1) + " dB", r, Justification::centred);
    }
}

void VoxBloomEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> list)
{
    const auto cellW = area.getWidth() / (int) list.size();
    for (auto* k : list)
    {
        auto cell = area.removeFromLeft (cellW).reduced (4, 0);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 96), cell.getHeight()));
    }
}

void VoxBloomEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Top bar
    auto top = area.removeFromTop (52);
    titleBounds = top.removeFromLeft (260);
    bpmBounds = top.removeFromRight (110);
    auto presetArea = top.withSizeKeepingCentre (440, 34);
    saveButton.setBounds (presetArea.removeFromRight (70));
    presetArea.removeFromRight (8);
    nextButton.setBounds (presetArea.removeFromRight (34));
    prevButton.setBounds (presetArea.removeFromLeft (34));
    presetArea.reduce (6, 0);
    presetBox.setBounds (presetArea);

    area.removeFromTop (10);
    const auto rowH = (area.getHeight() - 20) / 3;

    // Delay
    auto delayRow = area.removeFromTop (rowH);
    sections[0] = { "DELAY", delayRow };
    {
        auto inner = delayRow.reduced (12, 8);
        auto header = inner.removeFromTop (26);
        modeBox.setBounds (header.removeFromRight (120).reduced (0, 1));
        header.removeFromRight (8);
        syncButton.setBounds (header.removeFromRight (64).reduced (0, 1));
        inner.removeFromTop (4);

        // Time and Note share one slot; only one of them is visible at a time.
        layoutKnobs (inner, { timeKnob, knobs[2].get(), knobs[3].get(), knobs[4].get(),
                              knobs[5].get(), knobs[6].get(), knobs[7].get() });
        noteKnob->slider.setBounds (timeKnob->slider.getBounds());
        noteKnob->label.setBounds (timeKnob->label.getBounds());
        noteKnob->label.setVisible (false);
    }

    area.removeFromTop (10);
    auto reverbRow = area.removeFromTop (rowH);
    sections[1] = { "REVERB", reverbRow };
    {
        auto inner = reverbRow.reduced (12, 8);
        inner.removeFromTop (30);
        layoutKnobs (inner, { knobs[8].get(), knobs[9].get(), knobs[10].get(), knobs[11].get(),
                              knobs[12].get(), knobs[13].get(), knobs[14].get(), knobs[15].get() });
    }

    area.removeFromTop (10);
    auto bottom = area;
    auto duckRow = bottom.removeFromLeft (bottom.getWidth() / 2 - 5);
    bottom.removeFromLeft (10);
    sections[2] = { "DUCKING", duckRow };
    sections[3] = { "OUTPUT", bottom };
    {
        auto inner = duckRow.reduced (12, 8);
        inner.removeFromTop (30);
        duckMeterBounds = inner.removeFromRight (70);
        layoutKnobs (inner, { knobs[16].get(), knobs[17].get(), knobs[18].get() });
    }
    {
        auto inner = bottom.reduced (12, 8);
        inner.removeFromTop (30);
        layoutKnobs (inner, { knobs[19].get(), knobs[20].get(), knobs[21].get() });
    }
}
