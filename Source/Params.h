#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace vb
{
    // Parameter IDs. Never rename these once released: hosts store automation and
    // sessions by ID.
    namespace pid
    {
        inline const juce::String delaySync      { "delaySync" };
        inline const juce::String delayNote      { "delayNote" };
        inline const juce::String delayTime      { "delayTime" };
        inline const juce::String delayMode      { "delayMode" };
        inline const juce::String delayFeedback  { "delayFeedback" };
        inline const juce::String delayLowCut    { "delayLowCut" };
        inline const juce::String delayHighCut   { "delayHighCut" };
        inline const juce::String delayDrive     { "delayDrive" };
        inline const juce::String delayWobble    { "delayWobble" };
        inline const juce::String delayLevel     { "delayLevel" };

        inline const juce::String reverbSize     { "reverbSize" };
        inline const juce::String reverbDecay    { "reverbDecay" };
        inline const juce::String reverbPreDelay { "reverbPreDelay" };
        inline const juce::String reverbDamp     { "reverbDamp" };
        inline const juce::String reverbLowCut   { "reverbLowCut" };
        inline const juce::String reverbMod      { "reverbMod" };
        inline const juce::String reverbLevel    { "reverbLevel" };
        inline const juce::String reverbSource   { "reverbSource" };

        inline const juce::String duckAmount     { "duckAmount" };
        inline const juce::String duckThreshold  { "duckThreshold" };
        inline const juce::String duckRelease    { "duckRelease" };

        inline const juce::String width          { "width" };
        inline const juce::String mix            { "mix" };
        inline const juce::String output         { "output" };
    }

    enum class DelayMode { stereo = 0, pingPong, mono };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    const juce::StringArray& noteDivisionNames();
    double noteDivisionInBeats (int index);
    const juce::StringArray& delayModeNames();
}
