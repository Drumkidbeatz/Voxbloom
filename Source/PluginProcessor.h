#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EchoDelay.h"
#include "dsp/LushVerb.h"
#include "dsp/Ducker.h"
#include "Presets.h"

class VoxBloomProcessor final : public juce::AudioProcessor
{
public:
    VoxBloomProcessor();
    ~VoxBloomProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VoxBloom"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 25.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Presets (message thread only)
    void loadFactoryPreset (int index);
    bool loadUserPreset (const juce::File& file);
    bool saveUserPreset (const juce::String& name);
    juce::String getCurrentPresetName() const;

    /** Current ducking gain (1 = no ducking), for the editor's meter. */
    float getDuckGainForDisplay() const noexcept { return duckGainDisplay.load(); }
    double getCurrentBpm() const noexcept { return lastBpm.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateDspParameters();
    void processChunk (float* left, float* right, int numSamples, bool isMono);
    float delayTimeMs() const;

    vb::dsp::EchoDelay delay;
    vb::dsp::LushVerb  reverb;
    vb::dsp::Ducker    ducker;

    juce::AudioBuffer<float> scratch;   // delayL, delayR, verbInL, verbInR, verbL, verbR
    int maxChunk = 512;

    juce::SmoothedValue<float> delayLevel, reverbLevel, reverbSource, widthAmount, dryGain, wetGain, outGain;

    struct Raw
    {
        std::atomic<float>* delaySync, * delayNote, * delayTime, * delayMode, * delayFeedback,
                          * delayLowCut, * delayHighCut, * delayDrive, * delayWobble, * delayLevel,
                          * reverbSize, * reverbDecay, * reverbPreDelay, * reverbDamp, * reverbLowCut,
                          * reverbMod, * reverbLevel, * reverbSource,
                          * duckAmount, * duckThreshold, * duckRelease,
                          * width, * mix, * output;
    } raw {};

    std::atomic<double> lastBpm { 120.0 };
    std::atomic<float> duckGainDisplay { 1.0f };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBloomProcessor)
};
