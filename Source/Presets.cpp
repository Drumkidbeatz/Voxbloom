#include "Presets.h"
#include "Params.h"

namespace vb
{
const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace pid;

    // Note indices: 0 1/32, 1 1/16T, 2 1/16, 3 1/16D, 4 1/8T, 5 1/8, 6 1/8D,
    //               7 1/4T, 8 1/4, 9 1/4D, 10 1/2, 11 1/1
    // Mode indices: 0 Stereo, 1 Ping-Pong, 2 Mono
    static const std::vector<FactoryPreset> presets {
        { "Init", "Basic", {} },

        { "Dark Bloom", "Melodic", {
            { delayNote, 8 }, { delayMode, 1 }, { delayFeedback, 45 }, { delayLowCut, 320 },
            { delayHighCut, 3500 }, { delayDrive, 25 }, { delayWobble, 18 }, { delayLevel, 60 },
            { reverbSize, 85 }, { reverbDecay, 5.5f }, { reverbPreDelay, 40 }, { reverbDamp, 4500 },
            { reverbLowCut, 260 }, { reverbMod, 45 }, { reverbLevel, 50 }, { reverbSource, 60 },
            { duckAmount, 70 }, { duckThreshold, -32 }, { duckRelease, 600 },
            { width, 140 }, { mix, 40 } } },

        { "Melodic Hook", "Melodic", {
            { delayNote, 8 }, { delayMode, 1 }, { delayFeedback, 35 }, { delayLowCut, 250 },
            { delayHighCut, 5500 }, { delayDrive, 12 }, { delayWobble, 10 }, { delayLevel, 55 },
            { reverbSize, 70 }, { reverbDecay, 3.5f }, { reverbPreDelay, 30 }, { reverbDamp, 7000 },
            { reverbLowCut, 220 }, { reverbMod, 30 }, { reverbLevel, 45 }, { reverbSource, 35 },
            { duckAmount, 55 }, { duckThreshold, -30 }, { duckRelease, 400 },
            { width, 125 }, { mix, 32 } } },

        { "Night Drive Echo", "Melodic", {
            { delayNote, 6 }, { delayMode, 1 }, { delayFeedback, 50 }, { delayLowCut, 300 },
            { delayHighCut, 4500 }, { delayDrive, 20 }, { delayWobble, 22 }, { delayLevel, 65 },
            { reverbSize, 75 }, { reverbDecay, 4.0f }, { reverbPreDelay, 25 }, { reverbDamp, 5500 },
            { reverbLowCut, 250 }, { reverbMod, 40 }, { reverbLevel, 40 }, { reverbSource, 55 },
            { duckAmount, 65 }, { duckThreshold, -30 }, { duckRelease, 500 },
            { width, 150 }, { mix, 36 } } },

        { "Dotted Dream", "Melodic", {
            { delayNote, 6 }, { delayMode, 0 }, { delayFeedback, 42 }, { delayLowCut, 200 },
            { delayHighCut, 6500 }, { delayDrive, 8 }, { delayWobble, 40 }, { delayLevel, 55 },
            { reverbSize, 80 }, { reverbDecay, 4.5f }, { reverbPreDelay, 50 }, { reverbDamp, 8000 },
            { reverbLowCut, 200 }, { reverbMod, 60 }, { reverbLevel, 45 }, { reverbSource, 50 },
            { duckAmount, 50 }, { duckThreshold, -28 }, { duckRelease, 450 },
            { width, 160 }, { mix, 35 } } },

        { "Cloud Tail", "Ambient", {
            { delayNote, 10 }, { delayMode, 1 }, { delayFeedback, 38 }, { delayLowCut, 350 },
            { delayHighCut, 4000 }, { delayDrive, 10 }, { delayWobble, 25 }, { delayLevel, 45 },
            { reverbSize, 100 }, { reverbDecay, 12.0f }, { reverbPreDelay, 60 }, { reverbDamp, 5000 },
            { reverbLowCut, 300 }, { reverbMod, 70 }, { reverbLevel, 65 }, { reverbSource, 70 },
            { duckAmount, 80 }, { duckThreshold, -34 }, { duckRelease, 900 },
            { width, 170 }, { mix, 45 } } },

        { "Ambient Freeze", "Ambient", {
            { delayNote, 8 }, { delayMode, 1 }, { delayFeedback, 85 }, { delayLowCut, 400 },
            { delayHighCut, 3200 }, { delayDrive, 30 }, { delayWobble, 30 }, { delayLevel, 50 },
            { reverbSize, 100 }, { reverbDecay, 20.0f }, { reverbPreDelay, 80 }, { reverbDamp, 4000 },
            { reverbLowCut, 350 }, { reverbMod, 80 }, { reverbLevel, 60 }, { reverbSource, 80 },
            { duckAmount, 90 }, { duckThreshold, -36 }, { duckRelease, 1200 },
            { width, 180 }, { mix, 50 } } },

        { "Halftime Space", "Ambient", {
            { delayNote, 10 }, { delayMode, 1 }, { delayFeedback, 55 }, { delayLowCut, 280 },
            { delayHighCut, 5000 }, { delayDrive, 15 }, { delayWobble, 15 }, { delayLevel, 55 },
            { reverbSize, 90 }, { reverbDecay, 7.0f }, { reverbPreDelay, 45 }, { reverbDamp, 6000 },
            { reverbLowCut, 250 }, { reverbMod, 45 }, { reverbLevel, 50 }, { reverbSource, 50 },
            { duckAmount, 70 }, { duckThreshold, -30 }, { duckRelease, 700 },
            { width, 150 }, { mix, 40 } } },

        { "Tape Throw", "Throw", {
            { delayNote, 8 }, { delayMode, 0 }, { delayFeedback, 60 }, { delayLowCut, 420 },
            { delayHighCut, 3000 }, { delayDrive, 55 }, { delayWobble, 35 }, { delayLevel, 70 },
            { reverbSize, 55 }, { reverbDecay, 2.5f }, { reverbPreDelay, 20 }, { reverbDamp, 5000 },
            { reverbLowCut, 300 }, { reverbMod, 25 }, { reverbLevel, 25 }, { reverbSource, 60 },
            { duckAmount, 60 }, { duckThreshold, -28 }, { duckRelease, 350 },
            { width, 120 }, { mix, 40 } } },

        { "Ad-Lib Throw", "Throw", {
            { delayNote, 9 }, { delayMode, 1 }, { delayFeedback, 65 }, { delayLowCut, 350 },
            { delayHighCut, 4200 }, { delayDrive, 25 }, { delayWobble, 15 }, { delayLevel, 75 },
            { reverbSize, 80 }, { reverbDecay, 5.0f }, { reverbPreDelay, 30 }, { reverbDamp, 5500 },
            { reverbLowCut, 280 }, { reverbMod, 35 }, { reverbLevel, 45 }, { reverbSource, 70 },
            { duckAmount, 100 }, { duckThreshold, -40 }, { duckRelease, 450 },
            { width, 150 }, { mix, 60 } } },

        { "Phone Echo", "Lo-Fi", {
            { delayNote, 9 }, { delayMode, 1 }, { delayFeedback, 55 }, { delayLowCut, 900 },
            { delayHighCut, 2500 }, { delayDrive, 70 }, { delayWobble, 30 }, { delayLevel, 70 },
            { reverbSize, 50 }, { reverbDecay, 2.0f }, { reverbPreDelay, 15 }, { reverbDamp, 3500 },
            { reverbLowCut, 500 }, { reverbMod, 20 }, { reverbLevel, 25 }, { reverbSource, 70 },
            { duckAmount, 55 }, { duckThreshold, -30 }, { duckRelease, 400 },
            { width, 110 }, { mix, 38 } } },

        { "Wide Wash", "Wide", {
            { delayNote, 5 }, { delayMode, 1 }, { delayFeedback, 40 }, { delayLowCut, 250 },
            { delayHighCut, 6000 }, { delayDrive, 10 }, { delayWobble, 20 }, { delayLevel, 55 },
            { reverbSize, 85 }, { reverbDecay, 4.0f }, { reverbPreDelay, 35 }, { reverbDamp, 7500 },
            { reverbLowCut, 220 }, { reverbMod, 50 }, { reverbLevel, 50 }, { reverbSource, 45 },
            { duckAmount, 60 }, { duckThreshold, -30 }, { duckRelease, 500 },
            { width, 200 }, { mix, 38 } } },

        { "Slap & Hall", "Classic", {
            { delaySync, 0 }, { delayTime, 110 }, { delayMode, 2 }, { delayFeedback, 12 },
            { delayLowCut, 200 }, { delayHighCut, 7000 }, { delayDrive, 15 }, { delayWobble, 5 },
            { delayLevel, 45 }, { reverbSize, 70 }, { reverbDecay, 2.8f }, { reverbPreDelay, 25 },
            { reverbDamp, 7000 }, { reverbLowCut, 200 }, { reverbMod, 25 }, { reverbLevel, 45 },
            { reverbSource, 20 }, { duckAmount, 35 }, { duckThreshold, -28 }, { duckRelease, 300 },
            { width, 120 }, { mix, 30 } } },

        { "Tight Room", "Classic", {
            { delayNote, 2 }, { delayMode, 0 }, { delayFeedback, 15 }, { delayLowCut, 200 },
            { delayHighCut, 8000 }, { delayDrive, 5 }, { delayWobble, 5 }, { delayLevel, 25 },
            { reverbSize, 30 }, { reverbDecay, 1.2f }, { reverbPreDelay, 10 }, { reverbDamp, 9000 },
            { reverbLowCut, 180 }, { reverbMod, 15 }, { reverbLevel, 45 }, { reverbSource, 10 },
            { duckAmount, 25 }, { duckThreshold, -26 }, { duckRelease, 200 },
            { width, 110 }, { mix, 25 } } },
    };

    return presets;
}

juce::File UserPresets::folder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Drumkidbeatz").getChildFile ("VoxBloom").getChildFile ("Presets");
}

juce::Array<juce::File> UserPresets::list()
{
    auto files = folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    files.sort();
    return files;
}

bool UserPresets::save (const juce::String& name, const juce::ValueTree& state)
{
    const auto safeName = juce::File::createLegalFileName (name.trim());
    if (safeName.isEmpty())
        return false;

    auto dir = folder();
    if (! dir.createDirectory())
        return false;

    auto copy = state.createCopy();
    copy.setProperty ("presetName", name.trim(), nullptr);

    if (auto xml = copy.createXml())
        return xml->writeTo (dir.getChildFile (safeName + extension));

    return false;
}

juce::ValueTree UserPresets::load (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        return juce::ValueTree::fromXml (*xml);
    return {};
}
}
