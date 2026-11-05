#pragma once

#include "Effects.h"

namespace vb::dsp
{
    /** Tape-flavoured stereo / ping-pong delay.
        Every repeat passes through low cut, high cut and soft saturation, so echoes
        get darker, thinner and grittier as they fade out. */
    class EchoDelay
    {
    public:
        enum class Mode { stereo = 0, pingPong, mono };

        static constexpr double maxDelaySeconds = 4.0;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            const auto maxSamples = (int) std::ceil (sampleRate * (maxDelaySeconds + 0.05));
            lineL.prepare (maxSamples);
            lineR.prepare (maxSamples);
            timeGlide = smoothingCoeff (0.12, sampleRate);
            freezeGlide = smoothingCoeff (0.06, sampleRate);
            amountGlide = smoothingCoeff (0.05, sampleRate);
            shiftL.prepare (sampleRate);
            shiftR.prepare (sampleRate);
            reset();
        }

        void reset()
        {
            lineL.reset(); lineR.reset();
            for (auto* f : { &lpL, &lpR, &hpL, &hpR })
                f->reset();
            shiftL.reset(); shiftR.reset();
            currentDelay = targetDelay;
            lfoPhase = flutterPhase = 0.0;
            freezeAmount = freezeTarget;
            shimmer = shimmerTarget;
            holdL = holdR = 0.0f; holdCount = 0.0f;
        }

        /** Shimmer: part of every repeat is pitch shifted, so the echoes climb (or fall). */
        void setShimmer (float amount01, float semitones) noexcept
        {
            shimmerTarget = juce::jlimit (0.0f, 1.0f, amount01);
            shiftL.setSemitones (semitones);
            shiftR.setSemitones (semitones);
        }

        /** Bit depth and sample-rate reduction inside the loop: each repeat gets grittier. */
        void setLofi (float amount01) noexcept { lofi = juce::jlimit (0.0f, 1.0f, amount01); }

        /** Freeze: no new input, repeats stay forever without filtering. */
        void setFreeze (bool shouldFreeze) noexcept { freezeTarget = shouldFreeze ? 1.0f : 0.0f; }

        void setParameters (float timeMs, float feedback01, Mode newMode,
                            float lowCutHz, float highCutHz, float drive01, float wobble01) noexcept
        {
            targetDelay = juce::jlimit (2.0f, (float) (maxDelaySeconds * sampleRate),
                                        timeMs * 0.001f * (float) sampleRate);
            feedback = juce::jlimit (0.0f, 0.97f, feedback01);
            if (newMode != mode)
            {
                mode = newMode;
                lineL.reset(); lineR.reset();
            }
            lpL.setCutoff (highCutHz, sampleRate); lpR.setCutoff (highCutHz, sampleRate);
            hpL.setCutoff (lowCutHz, sampleRate);  hpR.setCutoff (lowCutHz, sampleRate);
            drive = juce::jlimit (0.0f, 1.0f, drive01);
            wobbleDepth = juce::jlimit (0.0f, 1.0f, wobble01) * 0.0035f * (float) sampleRate;
        }

        /** Output gain that compensates the level lost to saturation. */
        float makeupGain() const noexcept { return std::sqrt (1.0f + 3.0f * drive); }

        void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
        {
            const auto twoPi = juce::MathConstants<double>::twoPi;
            const auto lfoInc     = twoPi * 0.55 / sampleRate;
            const auto flutterInc = twoPi * 5.3 / sampleRate;
            const auto flutterDepth = wobbleDepth * 0.06f;
            const auto shiftLatency = shiftL.latencySamples();
            const auto lofiOn = lofi > 0.0f;
            const auto step = 2.0f / std::pow (2.0f, 16.0f - 12.0f * lofi);
            const auto holdLength = 1.0f + 11.0f * lofi;

            for (int i = 0; i < numSamples; ++i)
            {
                currentDelay += (targetDelay - currentDelay) * timeGlide;

                const auto flutter = flutterDepth * (float) std::sin (flutterPhase);
                const auto modL = wobbleDepth * (0.5f + 0.5f * (float) std::sin (lfoPhase)) + flutter;
                const auto modR = wobbleDepth * (0.5f + 0.5f * (float) std::sin (lfoPhase + 1.7)) + flutter;
                lfoPhase += lfoInc;      if (lfoPhase > twoPi) lfoPhase -= twoPi;
                flutterPhase += flutterInc; if (flutterPhase > twoPi) flutterPhase -= twoPi;

                freezeAmount += (freezeTarget - freezeAmount) * freezeGlide;
                shimmer += (shimmerTarget - shimmer) * amountGlide;
                const auto fb = feedback + (0.9999f - feedback) * freezeAmount;
                const auto inGain = 1.0f - freezeAmount;

                auto yL = lineL.read (currentDelay + modL);
                auto yR = lineR.read (currentDelay + modR);

                // The shifted repeats are read early by the shifter's latency so they
                // still land on the beat.
                auto fbL = yL, fbR = yR;
                const auto shimmerNow = shimmer * inGain;
                if (shimmerNow > 1.0e-4f)
                {
                    const auto early = juce::jmax (2.0f, currentDelay - shiftLatency);
                    const auto pL = shiftL.process (lineL.read (early + modL));
                    const auto pR = shiftR.process (lineR.read (early + modR));
                    fbL += shimmerNow * (pL - yL);
                    fbR += shimmerNow * (pR - yR);
                }

                const auto xL = inL[i] * inGain, xR = inR[i] * inGain;
                float sL, sR;

                switch (mode)
                {
                    case Mode::pingPong:
                        // Mono input enters the left line; the lines feed each other.
                        sL = 0.5f * (xL + xR) + fb * fbR;
                        sR = fb * fbL;
                        break;
                    case Mode::mono:
                        sL = sR = 0.5f * (xL + xR) + fb * fbL;
                        yR = yL;
                        break;
                    case Mode::stereo:
                    default:
                        sL = xL + fb * fbL;
                        sR = xR + fb * fbR;
                        break;
                }

                auto wL = softClip (hpL.highpass (lpL.lowpass (sL)), drive);
                auto wR = softClip (hpR.highpass (lpR.lowpass (sR)), drive);

                if (lofiOn)
                {
                    // Sample-and-hold for the rate reduction, then quantise.
                    holdCount += 1.0f;
                    if (holdCount >= holdLength)
                    {
                        holdCount -= holdLength;
                        holdL = std::round (wL / step) * step;
                        holdR = std::round (wR / step) * step;
                    }
                    wL = holdL;
                    wR = holdR;
                }

                // Frozen: bypass filters and grit so the loop neither darkens nor
                // dies away; tanh keeps it bounded.
                if (freezeAmount > 1.0e-4f)
                {
                    wL += freezeAmount * (std::tanh (sL) - wL);
                    wR += freezeAmount * (std::tanh (sR) - wR);
                }

                lineL.write (wL);
                lineR.write (wR);

                outL[i] = yL;
                outR[i] = yR;
            }
        }

    private:
        double sampleRate = 48000.0;
        FracDelay lineL, lineR;
        OnePole lpL, lpR, hpL, hpR;
        Mode mode = Mode::pingPong;
        float targetDelay = 1000.0f, currentDelay = 1000.0f, timeGlide = 0.001f;
        float feedback = 0.4f, drive = 0.0f, wobbleDepth = 0.0f;
        PitchShifter shiftL, shiftR;
        float shimmer = 0.0f, shimmerTarget = 0.0f, amountGlide = 0.001f;
        float lofi = 0.0f, holdL = 0.0f, holdR = 0.0f, holdCount = 0.0f;
        float freezeAmount = 0.0f, freezeTarget = 0.0f, freezeGlide = 0.001f;
        double lfoPhase = 0.0, flutterPhase = 0.0;
    };
}
