#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>

namespace vb
{
    /** A factory preset lists only the parameters that differ from the defaults,
        in real units (ms, Hz, %, choice index, 0/1 for switches). */
    struct FactoryPreset
    {
        juce::String name;
        juce::String category;
        std::vector<std::pair<juce::String, float>> values;
    };

    const std::vector<FactoryPreset>& factoryPresets();

    /** Folder for user presets (*.voxpreset) and helpers for them. */
    class UserPresets
    {
    public:
        static constexpr const char* extension = ".voxpreset";

        static juce::File folder();
        static juce::Array<juce::File> list();
        static bool save (const juce::String& name, const juce::ValueTree& state);
        static juce::ValueTree load (const juce::File& file);
    };
}
