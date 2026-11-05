#pragma once

#include "Primitives.h"
#include <array>

namespace vb::dsp
{
    /** 8-line feedback delay network reverb with input diffusion, in-loop damping,
        slow line modulation (for a lush, chorused tail) and a low cut on the output. */
    class LushVerb
    {
    public:
        static constexpr int numLines = 8;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            const auto ms = [this] (double m) { return (int) std::round (m * 0.001 * sampleRate); };

            for (auto& l : lines)
                l.prepare (ms (baseLengthsMs.back() * maxScale + 6.0));

            preL.prepare (ms (260.0));
            preR.prepare (ms (260.0));

            static constexpr double diffMs[] { 4.77, 3.59, 12.73, 9.31 };
            for (size_t i = 0; i < diffL.size(); ++i)
            {
                diffL[i].prepare (ms (diffMs[i]), 0.65f);
                diffR[i].prepare (ms (diffMs[i] * 1.071), 0.65f);
            }

            lengthGlide = smoothingCoeff (0.35, sampleRate);
            preGlide    = smoothingCoeff (0.08, sampleRate);
            reset();
        }

        void reset()
        {
            for (auto& l : lines) l.reset();
            for (auto& f : damp) f.reset();
            for (auto& d : diffL) d.reset();
            for (auto& d : diffR) d.reset();
            preL.reset(); preR.reset();
            hpL1.reset(); hpL2.reset(); hpR1.reset(); hpR2.reset();
            currentLengths = targetLengths;
            currentPre = targetPre;
            for (size_t i = 0; i < phases.size(); ++i)
                phases[i] = (double) i * 0.79;
        }

        void setParameters (float size01, float decaySeconds, float preDelayMs,
                            float dampHz, float lowCutHz, float mod01) noexcept
        {
            const auto scale = minScale + (maxScale - minScale) * juce::jlimit (0.0f, 1.0f, size01);
            for (size_t i = 0; i < numLines; ++i)
                targetLengths[i] = (float) (baseLengthsMs[i] * scale * 0.001 * sampleRate);

            decay = juce::jlimit (0.1f, 30.0f, decaySeconds);
            targetPre = juce::jmax (2.0f, preDelayMs * 0.001f * (float) sampleRate);
            for (auto& f : damp) f.setCutoff (dampHz, sampleRate);
            for (auto* f : { &hpL1, &hpL2, &hpR1, &hpR2 }) f->setCutoff (lowCutHz, sampleRate);
            modDepth = juce::jlimit (0.0f, 1.0f, mod01) * 0.0011f * (float) sampleRate;
        }

        void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
        {
            // Per-line feedback gains for the requested RT60, from the (smoothed) lengths.
            std::array<float, numLines> gains;
            for (size_t i = 0; i < numLines; ++i)
            {
                const auto lenSec = (currentLengths[i] + 0.5f * modDepth) / (float) sampleRate;
                gains[i] = std::pow (10.0f, -3.0f * lenSec / decay);
            }

            const auto twoPi = juce::MathConstants<double>::twoPi;
            static constexpr double rates[] { 0.31, 0.43, 0.57, 0.71, 0.37, 0.53, 0.67, 0.83 };
            static constexpr float signL[] { 1, -1, 1, -1, 1, 1, -1, -1 };
            static constexpr float signR[] { 1, 1, -1, -1, -1, 1, 1, -1 };
            const auto outScale = 0.6f / std::sqrt ((float) numLines);

            for (int n = 0; n < numSamples; ++n)
            {
                currentPre += (targetPre - currentPre) * preGlide;
                auto xL = preL.read (currentPre);
                auto xR = preR.read (currentPre);
                preL.write (inL[n]);
                preR.write (inR[n]);

                for (auto& d : diffL) xL = d.process (xL);
                for (auto& d : diffR) xR = d.process (xR);

                std::array<float, numLines> r;
                float sum = 0.0f;
                for (size_t i = 0; i < numLines; ++i)
                {
                    currentLengths[i] += (targetLengths[i] - currentLengths[i]) * lengthGlide;
                    const auto mod = modDepth * (0.5f + 0.5f * (float) std::sin (phases[i]));
                    phases[i] += twoPi * rates[i] / sampleRate;
                    if (phases[i] > twoPi) phases[i] -= twoPi;

                    r[i] = damp[i].lowpass (lines[i].read (currentLengths[i] + mod)) * gains[i];
                    sum += r[i];
                }

                // Householder mixing matrix: lossless, every line feeds every other line.
                const auto h = sum * (2.0f / (float) numLines);
                float yL = 0.0f, yR = 0.0f;
                for (size_t i = 0; i < numLines; ++i)
                {
                    const auto in = (i % 2 == 0 ? xL : xR) * 0.5f;
                    lines[i].write (r[i] - h + in);
                    yL += r[i] * signL[i];
                    yR += r[i] * signR[i];
                }

                outL[n] = hpL2.highpass (hpL1.highpass (yL * outScale));
                outR[n] = hpR2.highpass (hpR1.highpass (yR * outScale));
            }
        }

    private:
        static constexpr std::array<double, numLines> baseLengthsMs { 31.3, 37.9, 41.9, 47.3, 53.7, 61.1, 67.3, 73.9 };
        static constexpr double minScale = 0.35, maxScale = 1.6;

        double sampleRate = 48000.0;
        std::array<FracDelay, numLines> lines;
        std::array<OnePole, numLines> damp;
        std::array<Diffuser, 4> diffL, diffR;
        FracDelay preL, preR;
        OnePole hpL1, hpL2, hpR1, hpR2;

        std::array<float, numLines> targetLengths {}, currentLengths {};
        std::array<double, numLines> phases {};
        float targetPre = 2.0f, currentPre = 2.0f;
        float decay = 3.0f, modDepth = 0.0f, lengthGlide = 0.001f, preGlide = 0.001f;
    };
}
