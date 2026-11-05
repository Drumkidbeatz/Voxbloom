#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace vb::dsp
{
    /** Topology-preserving one-pole filter giving low-pass and high-pass outputs. */
    class OnePole
    {
    public:
        void setCutoff (float hz, double sampleRate) noexcept
        {
            const auto fc = juce::jlimit (5.0f, (float) (sampleRate * 0.45), hz);
            const auto g  = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
            G = g / (1.0f + g);
        }

        float lowpass (float x) noexcept
        {
            const auto v = (x - s) * G;
            const auto y = v + s;
            s = y + v;
            return y;
        }

        float highpass (float x) noexcept { return x - lowpass (x); }
        void reset() noexcept { s = 0.0f; }

    private:
        float G = 0.5f, s = 0.0f;
    };

    /** Circular delay line with cubic (Hermite) fractional reads.
        Call read() before write() for each sample; delays must be >= 2 samples. */
    class FracDelay
    {
    public:
        void prepare (int maxDelaySamples)
        {
            const auto size = juce::nextPowerOfTwo (juce::jmax (8, maxDelaySamples + 8));
            buffer.assign ((size_t) size, 0.0f);
            mask = size - 1;
            maxDelay = (float) (maxDelaySamples);
            writePos = 0;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
        }

        float read (float delaySamples) const noexcept
        {
            const auto d  = juce::jlimit (2.0f, maxDelay, delaySamples);
            const auto rp = (float) writePos - d;
            const auto fl = std::floor (rp);
            const auto i  = (int) fl;
            const auto f  = rp - fl;

            const auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const auto c1 = 0.5f * (x1 - xm1);
            const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        /** Integer read, cheaper; used by the diffusers. */
        float readInt (int delaySamples) const noexcept { return at (writePos - delaySamples); }

        void write (float x) noexcept
        {
            buffer[(size_t) writePos] = x;
            writePos = (writePos + 1) & mask;
        }

    private:
        float at (int i) const noexcept { return buffer[(size_t) (i & mask)]; }

        std::vector<float> buffer;
        int mask = 0, writePos = 0;
        float maxDelay = 2.0f;
    };

    /** Schroeder all-pass used to smear transients before the reverb tank. */
    class Diffuser
    {
    public:
        void prepare (int delaySamples, float gain)
        {
            length = juce::jmax (2, delaySamples);
            g = gain;
            line.prepare (length + 2);
        }

        void reset() noexcept { line.reset(); }

        float process (float x) noexcept
        {
            const auto d = line.readInt (length);
            const auto w = x + g * d;
            line.write (w);
            return d - g * w;
        }

    private:
        FracDelay line;
        int length = 2;
        float g = 0.6f;
    };

    /** Soft saturation with unity small-signal gain; also keeps feedback loops bounded. */
    inline float softClip (float x, float drive) noexcept
    {
        const auto g = 1.0f + 3.0f * drive;
        return std::tanh (x * g) / g;
    }

    inline float smoothingCoeff (double timeSeconds, double sampleRate) noexcept
    {
        return (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0e-4, timeSeconds) * sampleRate)));
    }
}
