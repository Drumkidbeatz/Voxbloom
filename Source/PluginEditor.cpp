#include "PluginEditor.h"
#include "Params.h"

using namespace vb;
namespace col = vb::ui::colours;

//==============================================================================
void ScopeView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto plot = r.reduced (12.0f, 8.0f);
    const auto n = VoxBloomProcessor::scopeSize;
    const auto dx = plot.getWidth() / (float) (n - 1);
    const auto toY = [&plot] (float level)
    {
        const auto db = Decibels::gainToDecibels (level, -60.0f);
        return plot.getBottom() - plot.getHeight() * jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };

    // Beat lines from the host tempo; the newest audio is on the right.
    const auto pointsPerBeat = 100.0 * 60.0 / jmax (20.0, processor.getCurrentBpm());
    g.setColour (col::panelEdge);
    for (double x = plot.getRight(); x > plot.getX(); x -= pointsPerBeat * dx)
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

    const auto newest = processor.getScopeWritePosition() - 1;
    Path wet, dry;
    wet.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i < n; ++i)
    {
        const auto p = processor.getScopePoint (newest - (n - 1) + i);
        const auto x = plot.getX() + (float) i * dx;
        wet.lineTo (x, toY (p.wet));
        if (i == 0) dry.startNewSubPath (x, toY (p.dry));
        else        dry.lineTo (x, toY (p.dry));
    }
    wet.lineTo (plot.getRight(), plot.getBottom());
    wet.closeSubPath();

    g.setGradientFill (ColourGradient (col::accent.withAlpha (0.75f), plot.getX(), plot.getBottom(),
                                       col::accent2.withAlpha (0.85f), plot.getRight(), plot.getY(), false));
    g.fillPath (wet);
    g.setColour (col::text.withAlpha (0.55f));
    g.strokePath (dry, PathStrokeType (1.2f));

    // Legend and status badges
    g.setFont (FontOptions (11.0f, Font::bold));
    g.setColour (col::text.withAlpha (0.55f));
    g.drawText ("VOCAL", plot.withHeight (14.0f), Justification::topLeft);
    g.setColour (col::accent2);
    g.drawText ("EFFECT", plot.withHeight (14.0f).withTrimmedLeft (50.0f), Justification::topLeft);

    auto badges = plot.withHeight (16.0f);
    auto badge = [&g, &badges] (const String& text, Colour c)
    {
        const auto b = badges.removeFromRight (78.0f);
        badges.removeFromRight (6.0f);
        g.setColour (c);
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (col::background);
        g.drawText (text, b, Justification::centred);
    };

    if (processor.isFrozenForDisplay())
        badge ("FROZEN", col::accent);
    if (const auto fc = processor.getSweepCutoffForDisplay(); fc > 0.0f)
        badge (fc < 1000.0f ? String (roundToInt (fc)) + " Hz" : String (fc / 1000.0f, 1) + " kHz", col::textDim);
}

//==============================================================================
VoxBloomEditor::VoxBloomEditor (VoxBloomProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (scope);

    using namespace pid;
    // Delay
    addKnob (delayTime,     "Time",      "Delay-Zeit in Millisekunden");
    addKnob (delayNote,     "Time",      "Delay-Zeit als Notenwert, passend zum Tempo der DAW");
    addKnob (delayFeedback, "Feedback",  "Wie oft das Echo wiederholt wird");
    addKnob (delayLowCut,   "Low Cut",   "Nimmt bei jeder Wiederholung Bass weg: Echos werden dünner");
    addKnob (delayHighCut,  "High Cut",  "Nimmt bei jeder Wiederholung Höhen weg: Echos werden dunkler");
    addKnob (delayDrive,    "Drive",     "Tape-Sättigung in den Wiederholungen");
    addKnob (delayWobble,   "Wobble",    "Leiern wie bei einer alten Bandmaschine");
    addKnob (lofi,          "Lo-Fi",     "Bitcrusher in der Rückkopplung: jede Wiederholung wird dreckiger");
    addKnob (shimmerAmount, "Shimmer",   "Anteil der Wiederholungen, die in der Tonhöhe verschoben werden");
    addKnob (delayLevel,    "Level",     "Lautstärke des Delays");
    // Reverb
    addKnob (reverbSize,     "Size",       "Raumgröße");
    addKnob (reverbDecay,    "Decay",      "Nachhallzeit");
    addKnob (reverbPreDelay, "Pre-Delay",  "Abstand zwischen Stimme und Hall");
    addKnob (reverbDamp,     "Damping",    "Höhen im Hallschweif");
    addKnob (reverbLowCut,   "Low Cut",    "Bass im Hall");
    addKnob (reverbMod,      "Modulation", "Chorus im Hallschweif");
    addKnob (reverse,        "Reverse",    "Hall rückwärts: der Schweif schwillt an statt auszuklingen");
    addKnob (reverseTime,    "Rev Time",   "Länge der rückwärts gespielten Abschnitte");
    addKnob (reverbSource,   "Delay > Verb", "0 %: Hall auf der Stimme, 100 %: Hall auf den Echos");
    addKnob (reverbLevel,    "Level",      "Lautstärke des Halls");
    // Sweep, gate, ducking
    addKnob (sweepRate,     "Rate",      "Geschwindigkeit des LFO");
    addKnob (sweepDepth,    "Depth",     "Wie weit der Filter den Effekt abdunkelt (0 % = aus)");
    addKnob (gateAmount,    "Amount",    "Schneidet den Hall ab, sobald die Stimme aufhört (0 % = aus)");
    addKnob (gateThreshold, "Threshold", "Ab welchem Pegel der Gate öffnet");
    addKnob (gateHold,      "Hold",      "Wie lange der Hall nach der Stimme noch stehen bleibt");
    addKnob (duckAmount,    "Amount",    "Wie stark der Effekt unter der Stimme abgesenkt wird");
    addKnob (duckThreshold, "Threshold", "Ab welchem Pegel abgesenkt wird");
    addKnob (duckRelease,   "Release",   "Wie schnell der Effekt nach der Stimme wiederkommt");
    // Doubler and output
    addKnob (doubler, "Amount", "Doppelt die Stimme links und rechts (Double-Tracking)");
    addKnob (width,   "Width",  "Stereobreite des Effekts");
    addKnob (mix,     "Mix",    "Bis 50 % bleibt die Stimme voll da, darüber wird sie leiser");
    addKnob (output,  "Output", "Ausgangspegel");

    addToggle (syncButton, delaySync, "Delay-Zeit zum Tempo der DAW synchronisieren");
    addToggle (freezeButton, freeze, "Hält Delay und Hall endlos fest, neue Töne kommen nicht mehr hinein");
    addToggle (throwModeButton, throwMode, "Der Effekt hört die Stimme nur, solange THROW gedrückt ist");
    addChoice (modeBox, delayMode, delayModeNames(), "Stereo, Ping-Pong oder Mono");
    addChoice (pitchBox, shimmerPitch, shimmerPitchNames(), "Tonhöhe der Shimmer-Wiederholungen");
    addChoice (sweepModeBox, sweepMode, sweepModeNames(), "LFO: Filter bewegt sich im Takt. Envelope: folgt der Lautstärke der Stimme");

    throwButton.setTooltip ("Gedrückt halten: dieses Wort geht in Delay und Hall (THROW MODE muss an sein)");
    throwButton.setTriggeredOnMouseDown (true);
    throwButton.onStateChange = [this] { setThrowHeld (throwButton.isDown()); };
    content.addAndMakeVisible (throwButton);

    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] { presetSelected(); };
    content.addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    slotA.onClick = [this] { processor.switchToSlot (0); timerCallback(); };
    slotB.onClick = [this] { processor.switchToSlot (1); timerCallback(); };
    copyButton.onClick = [this] { processor.copyActiveSlotToOther(); };
    slotA.setTooltip ("Einstellung A");
    slotB.setTooltip ("Einstellung B");
    copyButton.setTooltip ("Aktuelle Einstellung in den anderen Slot kopieren");
    for (auto* b : { &prevButton, &nextButton, &saveButton, &slotA, &slotB, &copyButton })
        content.addAndMakeVisible (b);

    for (auto z : { 75, 100, 125, 150, 200 })
        zoomBox.addItem (juce::String (z) + " %", z);
    zoomBox.setTooltip ("Fenstergröße. Du kannst das Fenster auch an der Ecke ziehen.");
    zoomBox.onChange = [this] { if (zoomBox.getSelectedId() > 0) setZoom ((float) zoomBox.getSelectedId() / 100.0f); };
    content.addAndMakeVisible (zoomBox);

    rebuildPresetMenu();

    // Read the stored size first: setting the limits resizes the (still empty) editor.
    const auto scale = processor.getEditorScale();
    content.setSize (baseWidth, baseHeight);
    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / baseHeight);

    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
    constructed = true;

    timerCallback();
    startTimerHz (30);
}

VoxBloomEditor::~VoxBloomEditor()
{
    stopTimer();
    setThrowHeld (false);
    if (saveDialog != nullptr)
        saveDialog->exitModalState (0);
    saveDialog.reset();
    setLookAndFeel (nullptr);
}

VoxBloomEditor::Knob& VoxBloomEditor::addKnob (const juce::String& paramId, const juce::String& caption, const juce::String& tooltip)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
    k->slider.setTooltip (tooltip);
    k->label.setText (caption, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    k->label.setColour (juce::Label::textColourId, col::textDim);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k->slider);

    // Set explicitly so the value box is rebuilt with these colours.
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    content.addAndMakeVisible (k->slider);
    content.addAndMakeVisible (k->label);
    knobs.emplace_back (paramId, std::move (k));
    return *knobs.back().second;
}

VoxBloomEditor::Knob* VoxBloomEditor::knob (const juce::String& paramId)
{
    for (auto& [id, k] : knobs)
        if (id == paramId)
            return k.get();
    jassertfalse;
    return nullptr;
}

void VoxBloomEditor::addToggle (juce::ToggleButton& b, const juce::String& paramId, const juce::String& tooltip)
{
    b.setTooltip (tooltip);
    content.addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, paramId, b));
}

void VoxBloomEditor::addChoice (juce::ComboBox& box, const juce::String& paramId, const juce::StringArray& items, const juce::String& tooltip)
{
    box.addItemList (items, 1);
    box.setTooltip (tooltip);
    content.addAndMakeVisible (box);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, paramId, box));
}

//==============================================================================
void VoxBloomEditor::setThrowHeld (bool held)
{
    if (held == throwHeld)
        return;

    throwHeld = held;
    if (auto* param = processor.apvts.getParameter (pid::throwActive))
    {
        if (held) param->beginChangeGesture();
        param->setValueNotifyingHost (held ? 1.0f : 0.0f);
        if (! held) param->endChangeGesture();
    }
}

void VoxBloomEditor::setZoom (float scale)
{
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
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
    const auto& params = processor.apvts;
    const auto synced = params.getRawParameterValue (pid::delaySync)->load() > 0.5f;
    knob (pid::delayTime)->slider.setVisible (! synced);
    knob (pid::delayNote)->slider.setVisible (synced);

    // Controls that only matter when their section is switched on are dimmed.
    const auto dim = [] (juce::Component& c, bool active) { c.setAlpha (active ? 1.0f : 0.45f); };
    const auto value = [&params] (const juce::String& id) { return params.getRawParameterValue (id)->load(); };
    dim (pitchBox, value (pid::shimmerAmount) > 0.0f);
    dim (knob (pid::reverseTime)->slider, value (pid::reverse) > 0.0f);
    for (auto* id : { &pid::sweepRate })
        dim (knob (*id)->slider, value (pid::sweepDepth) > 0.0f && value (pid::sweepMode) < 0.5f);
    dim (sweepModeBox, value (pid::sweepDepth) > 0.0f);
    for (auto* id : { &pid::gateThreshold, &pid::gateHold })
        dim (knob (*id)->slider, value (pid::gateAmount) > 0.0f);
    dim (throwButton, value (pid::throwMode) > 0.5f);
    throwButton.setToggleState (value (pid::throwActive) > 0.5f, juce::dontSendNotification);

    const auto slot = processor.getActiveSlot();
    slotA.setToggleState (slot == 0, juce::dontSendNotification);
    slotB.setToggleState (slot == 1, juce::dontSendNotification);
    copyButton.setButtonText (slot == 0 ? "A > B" : "B > A");

    const auto name = processor.getCurrentPresetName();
    if (presetBox.getText() != name && ! presetBox.isPopupActive())
        presetBox.setText (name, juce::dontSendNotification);

    const auto zoom = juce::roundToInt (100.0f * (float) getWidth() / (float) baseWidth);
    if (zoomBox.getSelectedId() != zoom && ! zoomBox.isPopupActive())
    {
        if (zoomBox.indexOfItemId (zoom) >= 0) zoomBox.setSelectedId (zoom, juce::dontSendNotification);
        else                                   zoomBox.setText (juce::String (zoom) + " %", juce::dontSendNotification);
    }

    scope.repaint();

    const auto duck = processor.getDuckGainForDisplay();
    const auto bpm  = processor.getCurrentBpm();
    if (std::abs (duck - duckDisplay) > 0.002f || std::abs (bpm - bpmDisplay) > 0.01)
    {
        duckDisplay = duck;
        bpmDisplay = bpm;
        content.repaint (duckMeterBounds.expanded (4));
        content.repaint (bpmBounds);
    }
}

//==============================================================================
void VoxBloomEditor::paintContent (juce::Graphics& g)
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
        if (s.bounds.isEmpty())
            continue;

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

void VoxBloomEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<const juce::String*> ids)
{
    const auto cellW = area.getWidth() / (int) ids.size();
    for (auto* id : ids)
    {
        auto* k = knob (*id);
        auto cell = area.removeFromLeft (cellW).reduced (3, 0);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 92), cell.getHeight()));
    }
}

void VoxBloomEditor::resized()
{
    const auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    if (constructed)
        processor.setEditorScale (scale);
}

void VoxBloomEditor::layoutContent()
{
    using namespace pid;
    auto area = content.getLocalBounds().reduced (16);

    // Top bar: title | A B copy | preset browser | zoom | tempo
    auto top = area.removeFromTop (52);
    titleBounds = top.removeFromLeft (250);
    bpmBounds = top.removeFromRight (80);
    zoomBox.setBounds (top.removeFromRight (86).withSizeKeepingCentre (86, 30));
    top.removeFromRight (12);
    {
        auto ab = top.removeFromLeft (160).withSizeKeepingCentre (160, 30);
        slotA.setBounds (ab.removeFromLeft (38));
        ab.removeFromLeft (4);
        slotB.setBounds (ab.removeFromLeft (38));
        ab.removeFromLeft (6);
        copyButton.setBounds (ab);
    }
    auto presetArea = top.withSizeKeepingCentre (juce::jmin (top.getWidth(), 440), 34);
    saveButton.setBounds (presetArea.removeFromRight (64));
    presetArea.removeFromRight (8);
    nextButton.setBounds (presetArea.removeFromRight (34));
    prevButton.setBounds (presetArea.removeFromLeft (34));
    presetArea.reduce (6, 0);
    presetBox.setBounds (presetArea);

    area.removeFromTop (10);
    scope.setBounds (area.removeFromTop (84));
    area.removeFromTop (10);

    const auto gap = 10;
    const auto rowH = (area.getHeight() - 3 * gap) / 4;
    const auto header = [] (juce::Rectangle<int> panel, juce::Rectangle<int>& headerOut)
    {
        auto inner = panel.reduced (12, 8);
        headerOut = inner.removeFromTop (26);
        inner.removeFromTop (4);
        return inner;
    };
    juce::Rectangle<int> head;

    // Row 1: delay
    auto delayRow = area.removeFromTop (rowH);
    sections[0] = { "DELAY", delayRow };
    {
        auto inner = header (delayRow, head);
        modeBox.setBounds (head.removeFromRight (110).reduced (0, 1));
        head.removeFromRight (8);
        syncButton.setBounds (head.removeFromRight (60).reduced (0, 1));
        head.removeFromRight (8);
        pitchBox.setBounds (head.removeFromRight (90).reduced (0, 1));

        // Time and Note share one slot; only one of them is visible at a time.
        layoutKnobs (inner, { &delayTime, &delayFeedback, &delayLowCut, &delayHighCut, &delayDrive,
                              &delayWobble, &lofi, &shimmerAmount, &delayLevel });
        auto* time = knob (delayTime);
        auto* note = knob (delayNote);
        note->slider.setBounds (time->slider.getBounds());
        note->label.setBounds (time->label.getBounds());
        note->label.setVisible (false);
    }

    // Row 2: reverb
    area.removeFromTop (gap);
    auto reverbRow = area.removeFromTop (rowH);
    sections[1] = { "REVERB", reverbRow };
    layoutKnobs (header (reverbRow, head), { &reverbSize, &reverbDecay, &reverbPreDelay, &reverbDamp, &reverbLowCut,
                                             &reverbMod, &reverse, &reverseTime, &reverbSource, &reverbLevel });

    // Row 3: sweep | gate | ducking
    area.removeFromTop (gap);
    auto row3 = area.removeFromTop (rowH);
    {
        const auto unit = (row3.getWidth() - 2 * gap) / 9;
        auto sweepPanel = row3.removeFromLeft (unit * 2 + 40);
        row3.removeFromLeft (gap);
        auto gatePanel = row3.removeFromLeft (unit * 3);
        row3.removeFromLeft (gap);
        auto duckPanel = row3;

        sections[2] = { "SWEEP", sweepPanel };
        auto inner = header (sweepPanel, head);
        sweepModeBox.setBounds (head.removeFromRight (100).reduced (0, 1));
        layoutKnobs (inner, { &sweepRate, &sweepDepth });

        sections[3] = { "GATE", gatePanel };
        layoutKnobs (header (gatePanel, head), { &gateAmount, &gateThreshold, &gateHold });

        sections[4] = { "DUCKING", duckPanel };
        inner = header (duckPanel, head);
        duckMeterBounds = inner.removeFromRight (64);
        layoutKnobs (inner, { &duckAmount, &duckThreshold, &duckRelease });
    }

    // Row 4: perform | doubler | output
    area.removeFromTop (gap);
    auto row4 = area;
    {
        auto performPanel = row4.removeFromLeft ((row4.getWidth() - 2 * gap) * 4 / 10);
        row4.removeFromLeft (gap);
        auto doublerPanel = row4.removeFromLeft (150);
        row4.removeFromLeft (gap);
        auto outputPanel = row4;

        sections[5] = { "PERFORM", performPanel };
        auto inner = header (performPanel, head).reduced (4, 6);
        const auto w = (inner.getWidth() - 2 * gap) / 3;
        freezeButton.setBounds (inner.removeFromLeft (w));
        inner.removeFromLeft (gap);
        throwModeButton.setBounds (inner.removeFromLeft (w));
        inner.removeFromLeft (gap);
        throwButton.setBounds (inner);

        sections[6] = { "DOUBLER", doublerPanel };
        layoutKnobs (header (doublerPanel, head), { &doubler });

        sections[7] = { "OUTPUT", outputPanel };
        layoutKnobs (header (outputPanel, head), { &width, &mix, &output });
    }
}
