#pragma once

#include "Primitives.h"

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
            reset();
        }

        void reset()
        {
            lineL.reset(); lineR.reset();
            for (auto* f : { &lpL, &lpR, &hpL, &hpR })
                f->reset();
            currentDelay = targetDelay;
            lfoPhase = flutterPhase = 0.0;
        }

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

            for (int i = 0; i < numSamples; ++i)
            {
                currentDelay += (targetDelay - currentDelay) * timeGlide;

                const auto flutter = flutterDepth * (float) std::sin (flutterPhase);
                const auto modL = wobbleDepth * (0.5f + 0.5f * (float) std::sin (lfoPhase)) + flutter;
                const auto modR = wobbleDepth * (0.5f + 0.5f * (float) std::sin (lfoPhase + 1.7)) + flutter;
                lfoPhase += lfoInc;      if (lfoPhase > twoPi) lfoPhase -= twoPi;
                flutterPhase += flutterInc; if (flutterPhase > twoPi) flutterPhase -= twoPi;

                auto yL = lineL.read (currentDelay + modL);
                auto yR = lineR.read (currentDelay + modR);

                const auto xL = inL[i], xR = inR[i];
                float sL, sR;

                switch (mode)
                {
                    case Mode::pingPong:
                        // Mono input enters the left line; the lines feed each other.
                        sL = 0.5f * (xL + xR) + feedback * yR;
                        sR = feedback * yL;
                        break;
                    case Mode::mono:
                        sL = sR = 0.5f * (xL + xR) + feedback * yL;
                        yR = yL;
                        break;
                    case Mode::stereo:
                    default:
                        sL = xL + feedback * yL;
                        sR = xR + feedback * yR;
                        break;
                }

                lineL.write (softClip (hpL.highpass (lpL.lowpass (sL)), drive));
                lineR.write (softClip (hpR.highpass (lpR.lowpass (sR)), drive));

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
        double lfoPhase = 0.0, flutterPhase = 0.0;
    };
}
