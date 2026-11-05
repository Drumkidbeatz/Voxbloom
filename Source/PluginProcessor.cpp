#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

using namespace vb;

namespace
{
    const juce::Identifier presetNameId { "presetName" };
    const juce::String abTag { "VoxBloomAB" };
    const juce::String uiScaleAttr { "uiScale" };

    std::atomic<float>* rawParam (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    }

    /** Mix curve that keeps the vocal at full level up to 50 % and only then
        starts to pull it back, which is what you want on an insert. */
    std::pair<float, float> mixGains (float mix01)
    {
        const auto dry = mix01 <= 0.5f ? 1.0f : (1.0f - mix01) * 2.0f;
        const auto wet = mix01 >= 0.5f ? 1.0f : mix01 * 2.0f;
        return { dry, wet };
    }
}

VoxBloomProcessor::VoxBloomProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxBloom", createParameterLayout())
{
    raw = { rawParam (apvts, pid::delaySync), rawParam (apvts, pid::delayNote), rawParam (apvts, pid::delayTime),
            rawParam (apvts, pid::delayMode), rawParam (apvts, pid::delayFeedback), rawParam (apvts, pid::delayLowCut),
            rawParam (apvts, pid::delayHighCut), rawParam (apvts, pid::delayDrive), rawParam (apvts, pid::delayWobble),
            rawParam (apvts, pid::delayLevel),
            rawParam (apvts, pid::reverbSize), rawParam (apvts, pid::reverbDecay), rawParam (apvts, pid::reverbPreDelay),
            rawParam (apvts, pid::reverbDamp), rawParam (apvts, pid::reverbLowCut), rawParam (apvts, pid::reverbMod),
            rawParam (apvts, pid::reverbLevel), rawParam (apvts, pid::reverbSource),
            rawParam (apvts, pid::duckAmount), rawParam (apvts, pid::duckThreshold), rawParam (apvts, pid::duckRelease),
            rawParam (apvts, pid::shimmerAmount), rawParam (apvts, pid::shimmerPitch), rawParam (apvts, pid::lofi),
            rawParam (apvts, pid::reverse), rawParam (apvts, pid::reverseTime),
            rawParam (apvts, pid::sweepMode), rawParam (apvts, pid::sweepRate), rawParam (apvts, pid::sweepDepth),
            rawParam (apvts, pid::gateAmount), rawParam (apvts, pid::gateThreshold), rawParam (apvts, pid::gateHold),
            rawParam (apvts, pid::freeze), rawParam (apvts, pid::throwMode), rawParam (apvts, pid::throwActive),
            rawParam (apvts, pid::doubler),
            rawParam (apvts, pid::width), rawParam (apvts, pid::mix), rawParam (apvts, pid::output) };

    apvts.state.setProperty (presetNameId, factoryPresets().front().name, nullptr);
}

bool VoxBloomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void VoxBloomProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxChunk = juce::jlimit (32, 4096, samplesPerBlock);
    scratch.setSize (8, maxChunk, false, true, false);

    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    ducker.prepare (sampleRate);
    reverseL.prepare (sampleRate);
    reverseR.prepare (sampleRate);
    doubler.prepare (sampleRate);
    sweep.prepare (sampleRate);
    gate.prepare (sampleRate);

    const auto smoothers = { &delayLevel, &reverbLevel, &reverbSource, &widthAmount, &dryGain, &wetGain, &outGain,
                             &sendGain, &reverseMix, &gateDepth, &doublerMix };
    for (auto* s : smoothers)
        s->reset (sampleRate, 0.05);
    sendGain.reset (sampleRate, 0.006);   // fast enough to catch a single word

    updateDspParameters();
    for (auto* s : smoothers)
        s->setCurrentAndTargetValue (s->getTargetValue());

    scopeInterval = juce::jmax (1, (int) (sampleRate * 0.01));
    scopeCounter = 0;

    reset();
}

void VoxBloomProcessor::reset()
{
    delay.reset();
    reverb.reset();
    ducker.reset();
    reverseL.reset();
    reverseR.reset();
    doubler.reset();
    sweep.reset();
    gate.reset();
}

float VoxBloomProcessor::delayTimeMs() const
{
    if (raw.delaySync->load() < 0.5f)
        return raw.delayTime->load();

    const auto beats = noteDivisionInBeats ((int) raw.delayNote->load());
    const auto ms = beats * 60000.0 / lastBpm.load();
    return (float) juce::jmin (ms, dsp::EchoDelay::maxDelaySeconds * 1000.0);
}

void VoxBloomProcessor::updateDspParameters()
{
    delay.setParameters (delayTimeMs(),
                         raw.delayFeedback->load() * 0.01f,
                         (dsp::EchoDelay::Mode) (int) raw.delayMode->load(),
                         raw.delayLowCut->load(),
                         raw.delayHighCut->load(),
                         raw.delayDrive->load() * 0.01f,
                         raw.delayWobble->load() * 0.01f);

    reverb.setParameters (raw.reverbSize->load() * 0.01f,
                          raw.reverbDecay->load(),
                          raw.reverbPreDelay->load(),
                          raw.reverbDamp->load(),
                          raw.reverbLowCut->load(),
                          raw.reverbMod->load() * 0.01f);

    ducker.setParameters (raw.duckAmount->load() * 0.01f, raw.duckThreshold->load(), raw.duckRelease->load());

    const auto frozen = raw.freeze->load() > 0.5f;
    delay.setShimmer (raw.shimmerAmount->load() * 0.01f, shimmerSemitones ((int) raw.shimmerPitch->load()));
    delay.setLofi (raw.lofi->load() * 0.01f);
    delay.setFreeze (frozen);
    reverb.setFreeze (frozen);
    reverseL.setLength (raw.reverseTime->load());
    reverseR.setLength (raw.reverseTime->load());
    sweep.setParameters ((dsp::SweepFilter::Mode) (int) raw.sweepMode->load(),
                         raw.sweepRate->load(), raw.sweepDepth->load() * 0.01f);
    gate.setParameters (raw.gateThreshold->load(), raw.gateHold->load());

    // Throw mode: the effects only hear the vocal while Throw is held.
    const auto throwing = raw.throwMode->load() < 0.5f || raw.throwActive->load() > 0.5f;
    sendGain.setTargetValue (throwing ? 1.0f : 0.0f);
    reverseMix.setTargetValue (raw.reverse->load() * 0.01f);
    // A frozen tail should not be chopped by the gate.
    gateDepth.setTargetValue (frozen ? 0.0f : raw.gateAmount->load() * 0.01f);
    doublerMix.setTargetValue (raw.doubler->load() * 0.01f);

    const auto [dry, wet] = mixGains (raw.mix->load() * 0.01f);
    delayLevel.setTargetValue (raw.delayLevel->load() * 0.01f * delay.makeupGain());
    reverbLevel.setTargetValue (raw.reverbLevel->load() * 0.01f);
    reverbSource.setTargetValue (raw.reverbSource->load() * 0.01f);
    widthAmount.setTargetValue (raw.width->load() * 0.01f);
    dryGain.setTargetValue (dry);
    wetGain.setTargetValue (wet);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (raw.output->load()));
}

void VoxBloomProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (numOut == 0)
        return;

    // Mono input into a stereo output: duplicate so the effect can widen it.
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 20.0 && *bpm < 999.0)
                lastBpm.store (*bpm);

    updateDspParameters();

    const auto isMono = numOut == 1;
    auto* left  = buffer.getWritePointer (0);
    auto* right = isMono ? left : buffer.getWritePointer (1);

    for (int start = 0; start < buffer.getNumSamples(); start += maxChunk)
    {
        const auto n = juce::jmin (maxChunk, buffer.getNumSamples() - start);
        processChunk (left + start, right + start, n, isMono);
    }

    duckGainDisplay.store (ducker.currentGain());
    sweepCutoffDisplay.store (raw.sweepDepth->load() > 0.0f ? sweep.currentCutoff() : 0.0f);
    frozenDisplay.store (raw.freeze->load() > 0.5f);
}

void VoxBloomProcessor::processChunk (float* left, float* right, int n, bool isMono)
{
    auto* dL = scratch.getWritePointer (0);
    auto* dR = scratch.getWritePointer (1);
    auto* iL = scratch.getWritePointer (2);
    auto* iR = scratch.getWritePointer (3);
    auto* vL = scratch.getWritePointer (4);
    auto* vR = scratch.getWritePointer (5);
    auto* sL = scratch.getWritePointer (6);
    auto* sR = scratch.getWritePointer (7);

    // What the effects hear: the vocal, or nothing outside a throw.
    for (int i = 0; i < n; ++i)
    {
        const auto send = sendGain.getNextValue();
        sL[i] = left[i] * send;
        sR[i] = right[i] * send;
    }

    delay.process (sL, sR, dL, dR, n);

    for (int i = 0; i < n; ++i)
    {
        const auto src = reverbSource.getNextValue();
        iL[i] = sL[i] * (1.0f - src) + dL[i] * src;
        iR[i] = sR[i] * (1.0f - src) + dR[i] * src;
    }

    reverb.process (iL, iR, vL, vR, n);

    for (int i = 0; i < n; ++i)
    {
        const auto dLev = delayLevel.getNextValue();
        const auto rLev = reverbLevel.getNextValue();
        const auto w    = widthAmount.getNextValue();
        const auto dry  = dryGain.getNextValue();
        const auto wet  = wetGain.getNextValue();
        const auto out  = outGain.getNextValue();
        const auto rev  = reverseMix.getNextValue();
        const auto gd   = gateDepth.getNextValue();
        const auto dbl  = doublerMix.getNextValue();

        const auto detector = juce::jmax (std::abs (left[i]), std::abs (right[i]));

        // Reverse swell: the reverb played backwards in chunks.
        auto rvL = vL[i], rvR = vR[i];
        const auto bL = reverseL.process (rvL);
        const auto bR = reverseR.process (rvR);
        rvL += rev * (bL - rvL);
        rvR += rev * (bR - rvR);

        // Gate on the reverb: open with the vocal, cut after the hold time.
        const auto gateGain = 1.0f - gd * (1.0f - gate.process (detector));

        auto wL = dL[i] * dLev + rvL * rLev * gateGain;
        auto wR = dR[i] * dLev + rvR * rLev * gateGain;

        sweep.process (wL, wR, detector);

        // Mid/side width on the wet signal only; the vocal itself stays centred.
        const auto mid  = 0.5f * (wL + wR);
        const auto side = 0.5f * (wL - wR) * w;
        wL = mid + side;
        wR = mid - side;

        const auto duck = ducker.process (detector);
        const auto wetGainNow = wet * duck;

        // Doubler: two detuned copies of the vocal, panned hard left and right.
        float xL = left[i], xR = right[i];
        float dblL, dblR;
        doubler.process (xL, xR, dblL, dblR);
        xL += dbl * 0.7f * dblL;
        xR += dbl * 0.7f * dblR;

        const auto peakDry = detector * dry;
        float peakWet;

        if (isMono)
        {
            const auto wetMono = 0.5f * (wL + wR) * wetGainNow;
            left[i] = (0.5f * (xL + xR) * dry + wetMono) * out;
            peakWet = std::abs (wetMono);
        }
        else
        {
            left[i]  = (xL * dry + wL * wetGainNow) * out;
            right[i] = (xR * dry + wR * wetGainNow) * out;
            peakWet = juce::jmax (std::abs (wL), std::abs (wR)) * wetGainNow;
        }

        scopePeakDry = juce::jmax (scopePeakDry, peakDry * out);
        scopePeakWet = juce::jmax (scopePeakWet, peakWet * out);
        if (++scopeCounter >= scopeInterval)
        {
            const auto pos = scopeWrite.load (std::memory_order_relaxed);
            scopeDry[(size_t) pos].store (scopePeakDry, std::memory_order_relaxed);
            scopeWet[(size_t) pos].store (scopePeakWet, std::memory_order_relaxed);
            scopeWrite.store ((pos + 1) % scopeSize, std::memory_order_release);
            scopeCounter = 0;
            scopePeakDry = scopePeakWet = 0.0f;
        }
    }
}

juce::AudioProcessorEditor* VoxBloomProcessor::createEditor()
{
    return new VoxBloomEditor (*this);
}

//==============================================================================
int VoxBloomProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

void VoxBloomProcessor::setCurrentProgram (int index)
{
    loadFactoryPreset (index);
}

const juce::String VoxBloomProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void VoxBloomProcessor::loadFactoryPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    const auto& preset = presets[(size_t) index];

    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            continue;

        auto normalised = ranged->getDefaultValue();
        for (const auto& [id, value] : preset.values)
            if (id == ranged->getParameterID())
                normalised = ranged->convertTo0to1 (value);

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (normalised);
        ranged->endChangeGesture();
    }

    currentProgram = index;
    apvts.state.setProperty (presetNameId, preset.name, nullptr);
}

bool VoxBloomProcessor::loadUserPreset (const juce::File& file)
{
    auto tree = UserPresets::load (file);
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        return false;

    const auto name = tree.getProperty (presetNameId, file.getFileNameWithoutExtension()).toString();
    apvts.replaceState (tree);
    apvts.state.setProperty (presetNameId, name, nullptr);
    return true;
}

bool VoxBloomProcessor::saveUserPreset (const juce::String& name)
{
    if (! UserPresets::save (name, apvts.copyState()))
        return false;

    apvts.state.setProperty (presetNameId, name.trim(), nullptr);
    return true;
}

juce::String VoxBloomProcessor::getCurrentPresetName() const
{
    return apvts.state.getProperty (presetNameId).toString();
}

void VoxBloomProcessor::switchToSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot)
        return;

    slots[(size_t) activeSlot] = apvts.copyState();
    activeSlot = slot;

    // A slot that was never used starts as a copy of the other one.
    if (slots[(size_t) slot].isValid())
        apvts.replaceState (slots[(size_t) slot].createCopy());
}

void VoxBloomProcessor::copyActiveSlotToOther()
{
    slots[(size_t) (1 - activeSlot)] = apvts.copyState();
}

void VoxBloomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr)
        return;

    // Momentary controls should not come back "pressed" when a session is reopened.
    for (auto* child : xml->getChildIterator())
        if (child->getStringAttribute ("id") == pid::throwActive)
            child->setAttribute ("value", 0.0);

    xml->setAttribute (uiScaleAttr, (double) editorScale);

    auto* ab = xml->createNewChildElement (abTag);
    ab->setAttribute ("active", activeSlot);
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i].isValid() && (int) i != activeSlot)
            if (auto slotXml = slots[i].createXml())
            {
                slotXml->setAttribute ("slot", (int) i);
                ab->addChildElement (slotXml.release());
            }

    copyXmlToBinary (*xml, destData);
}

void VoxBloomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    editorScale = juce::jlimit (0.6f, 2.0f, (float) xml->getDoubleAttribute (uiScaleAttr, editorScale));
    xml->removeAttribute (uiScaleAttr);

    slots = {};
    activeSlot = 0;
    if (auto* ab = xml->getChildByName (abTag))
    {
        activeSlot = juce::jlimit (0, 1, ab->getIntAttribute ("active"));
        for (auto* slotXml : ab->getChildIterator())
        {
            const auto index = slotXml->getIntAttribute ("slot", -1);
            if (index == 0 || index == 1)
            {
                slotXml->removeAttribute ("slot");
                slots[(size_t) index] = juce::ValueTree::fromXml (*slotXml);
            }
        }
        xml->removeChildElement (ab, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxBloomProcessor();
}
