#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

using namespace vb;

namespace
{
    const juce::Identifier presetNameId { "presetName" };

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
    scratch.setSize (6, maxChunk, false, true, false);

    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    ducker.prepare (sampleRate);

    for (auto* s : { &delayLevel, &reverbLevel, &reverbSource, &widthAmount, &dryGain, &wetGain, &outGain })
        s->reset (sampleRate, 0.05);

    updateDspParameters();
    for (auto* s : { &delayLevel, &reverbLevel, &reverbSource, &widthAmount, &dryGain, &wetGain, &outGain })
        s->setCurrentAndTargetValue (s->getTargetValue());

    reset();
}

void VoxBloomProcessor::reset()
{
    delay.reset();
    reverb.reset();
    ducker.reset();
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
}

void VoxBloomProcessor::processChunk (float* left, float* right, int n, bool isMono)
{
    auto* dL = scratch.getWritePointer (0);
    auto* dR = scratch.getWritePointer (1);
    auto* iL = scratch.getWritePointer (2);
    auto* iR = scratch.getWritePointer (3);
    auto* vL = scratch.getWritePointer (4);
    auto* vR = scratch.getWritePointer (5);

    delay.process (left, right, dL, dR, n);

    for (int i = 0; i < n; ++i)
    {
        const auto src = reverbSource.getNextValue();
        iL[i] = left[i]  * (1.0f - src) + dL[i] * src;
        iR[i] = right[i] * (1.0f - src) + dR[i] * src;
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

        auto wL = dL[i] * dLev + vL[i] * rLev;
        auto wR = dR[i] * dLev + vR[i] * rLev;

        // Mid/side width on the wet signal only; the vocal itself stays centred.
        const auto mid  = 0.5f * (wL + wR);
        const auto side = 0.5f * (wL - wR) * w;
        wL = mid + side;
        wR = mid - side;

        const auto duck = ducker.process (juce::jmax (std::abs (left[i]), std::abs (right[i])));
        const auto wetGainNow = wet * duck;

        if (isMono)
        {
            left[i] = (left[i] * dry + 0.5f * (wL + wR) * wetGainNow) * out;
        }
        else
        {
            left[i]  = (left[i]  * dry + wL * wetGainNow) * out;
            right[i] = (right[i] * dry + wR * wetGainNow) * out;
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

void VoxBloomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxBloomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxBloomProcessor();
}
