#pragma once

#include "Primitives.h"

namespace vb::dsp
{
    /** Pushes the wet signal down while the vocal is sounding and lets it bloom in
        the gaps. Feed it the dry input level; apply the returned gain to the wet. */
    class Ducker
    {
    public:
        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            envAttack   = smoothingCoeff (0.002, sampleRate);
            envRelease  = smoothingCoeff (0.080, sampleRate);
            gainAttack  = smoothingCoeff (0.015, sampleRate);
            reset();
        }

        void reset() noexcept { envelope = 0.0f; gain = 1.0f; }

        void setParameters (float amount01, float thresholdDb, float releaseMs) noexcept
        {
            amount = juce::jlimit (0.0f, 1.0f, amount01);
            threshold = thresholdDb;
            gainRelease = smoothingCoeff (releaseMs * 0.001, sampleRate);
        }

        float process (float detectorAbs) noexcept
        {
            envelope += (detectorAbs - envelope) * (detectorAbs > envelope ? envAttack : envRelease);

            const auto levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
            const auto over = juce::jlimit (0.0f, 1.0f, (levelDb - threshold) / 12.0f);
            const auto target = 1.0f - amount * over;

            gain += (target - gain) * (target < gain ? gainAttack : gainRelease);
            return gain;
        }

        float currentGain() const noexcept { return gain; }

    private:
        double sampleRate = 48000.0;
        float envelope = 0.0f, gain = 1.0f;
        float amount = 0.0f, threshold = -30.0f;
        float envAttack = 0.1f, envRelease = 0.01f, gainAttack = 0.01f, gainRelease = 0.001f;
    };
}
