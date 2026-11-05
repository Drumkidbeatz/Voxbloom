#include "Params.h"

namespace vb
{
namespace
{
    using Fn = std::function<juce::String (float, int)>;

    juce::String formatPercent (float v, int)  { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String formatMs (float v, int)       { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String formatSeconds (float v, int)  { return juce::String (v, v < 10.0f ? 2 : 1) + " s"; }
    juce::String formatDb (float v, int)       { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }

    juce::String formatHz (float v, int)
    {
        if (v < 1000.0f)
            return juce::String (juce::roundToInt (v)) + " Hz";
        return juce::String (v / 1000.0f, 1) + " kHz";
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id,
                                                          const juce::String& name,
                                                          juce::NormalisableRange<float> range,
                                                          float defaultValue,
                                                          Fn toText)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText)));
    }
}

const juce::StringArray& noteDivisionNames()
{
    static const juce::StringArray names { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                           "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    return names;
}

double noteDivisionInBeats (int index)
{
    static constexpr double beats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5,
                                      0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

const juce::StringArray& delayModeNames()
{
    static const juce::StringArray names { "Stereo", "Ping-Pong", "Mono" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto delay = std::make_unique<AudioProcessorParameterGroup> ("delay", "Delay", " | ");
    delay->addChild (std::make_unique<AudioParameterBool> (ParameterID { pid::delaySync, 1 }, "Delay Sync", true),
                     std::make_unique<AudioParameterChoice> (ParameterID { pid::delayNote, 1 }, "Delay Note", noteDivisionNames(), 8),
                     makeFloat (pid::delayTime, "Delay Time", skewed (1.0f, 2000.0f, 300.0f), 350.0f, formatMs),
                     std::make_unique<AudioParameterChoice> (ParameterID { pid::delayMode, 1 }, "Delay Mode", delayModeNames(), 1),
                     makeFloat (pid::delayFeedback, "Feedback", { 0.0f, 95.0f }, 40.0f, formatPercent),
                     makeFloat (pid::delayLowCut, "Delay Low Cut", skewed (20.0f, 2000.0f, 300.0f), 250.0f, formatHz),
                     makeFloat (pid::delayHighCut, "Delay High Cut", skewed (1000.0f, 20000.0f, 5000.0f), 6000.0f, formatHz),
                     makeFloat (pid::delayDrive, "Drive", { 0.0f, 100.0f }, 15.0f, formatPercent),
                     makeFloat (pid::delayWobble, "Wobble", { 0.0f, 100.0f }, 10.0f, formatPercent),
                     makeFloat (pid::delayLevel, "Delay Level", { 0.0f, 100.0f }, 60.0f, formatPercent));

    auto reverb = std::make_unique<AudioProcessorParameterGroup> ("reverb", "Reverb", " | ");
    reverb->addChild (makeFloat (pid::reverbSize, "Size", { 0.0f, 100.0f }, 70.0f, formatPercent),
                      makeFloat (pid::reverbDecay, "Decay", skewed (0.3f, 20.0f, 3.0f), 3.5f, formatSeconds),
                      makeFloat (pid::reverbPreDelay, "Pre-Delay", { 0.0f, 250.0f }, 30.0f, formatMs),
                      makeFloat (pid::reverbDamp, "Damping", skewed (1000.0f, 20000.0f, 6000.0f), 7000.0f, formatHz),
                      makeFloat (pid::reverbLowCut, "Reverb Low Cut", skewed (20.0f, 1000.0f, 200.0f), 200.0f, formatHz),
                      makeFloat (pid::reverbMod, "Modulation", { 0.0f, 100.0f }, 30.0f, formatPercent),
                      makeFloat (pid::reverbLevel, "Reverb Level", { 0.0f, 100.0f }, 45.0f, formatPercent),
                      makeFloat (pid::reverbSource, "Delay > Reverb", { 0.0f, 100.0f }, 40.0f, formatPercent));

    auto duck = std::make_unique<AudioProcessorParameterGroup> ("duck", "Ducking", " | ");
    duck->addChild (makeFloat (pid::duckAmount, "Duck Amount", { 0.0f, 100.0f }, 50.0f, formatPercent),
                    makeFloat (pid::duckThreshold, "Duck Threshold", { -60.0f, 0.0f }, -30.0f, formatDb),
                    makeFloat (pid::duckRelease, "Duck Release", skewed (20.0f, 2000.0f, 300.0f), 400.0f, formatMs));

    auto out = std::make_unique<AudioProcessorParameterGroup> ("output", "Output", " | ");
    out->addChild (makeFloat (pid::width, "Width", { 0.0f, 200.0f }, 120.0f, formatPercent),
                   makeFloat (pid::mix, "Mix", { 0.0f, 100.0f }, 35.0f, formatPercent),
                   makeFloat (pid::output, "Output", { -24.0f, 12.0f }, 0.0f, formatDb));

    layout.add (std::move (delay), std::move (reverb), std::move (duck), std::move (out));
    return layout;
}
}
