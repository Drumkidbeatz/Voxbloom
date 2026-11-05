#pragma once

#include "Primitives.h"

namespace vb::dsp
{
    /** Two-head granular pitch shifter. Each head sweeps through a short window
        of the delay line at the new speed; Hann windows offset by half a cycle
        cross-fade the heads so their gains always sum to one.
        Average latency is half the window (windowSeconds / 2). */
    class PitchShifter
    {
    public:
        static constexpr double windowSeconds = 0.06;

        void prepare (double sampleRate)
        {
            window = (float) (windowSeconds * sampleRate);
            line.prepare ((int) window + 8);
            reset();
        }

        void reset() noexcept { line.reset(); phase = 0.0f; }

        void setSemitones (float semis) noexcept
        {
            const auto ratio = std::pow (2.0f, semis / 12.0f);
            increment = (1.0f - ratio) / window;
        }

        float latencySamples() const noexcept { return 0.5f * window + 2.0f; }

        float process (float x) noexcept
        {
            phase += increment;
            phase -= std::floor (phase);

            auto p2 = phase + 0.5f;
            if (p2 >= 1.0f) p2 -= 1.0f;

            const auto pi = juce::MathConstants<float>::pi;
            const auto w1 = std::sin (pi * phase), w2 = std::sin (pi * p2);
            const auto y = w1 * w1 * line.read (2.0f + phase * window)
                         + w2 * w2 * line.read (2.0f + p2 * window);
            line.write (x);
            return y;
        }

    private:
        FracDelay line;
        float window = 2880.0f, phase = 0.0f, increment = 0.0f;
    };

    /** Plays the signal backwards in overlapping chunks: each chunk swells up
        instead of decaying, which turns a reverb tail into a reverse swell. */
    class ReverseDelay
    {
    public:
        static constexpr double maxSeconds = 2.0;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            line.prepare ((int) std::ceil (2.0 * maxSeconds * sampleRate) + 8);
            reset();
        }

        void reset() noexcept { line.reset(); phase = 0.0f; }

        void setLength (float ms) noexcept
        {
            pending = juce::jlimit (0.02f, (float) maxSeconds, ms * 0.001f) * (float) sampleRate;
        }

        float process (float x) noexcept
        {
            // The chunk length only changes at a chunk boundary so the heads never jump.
            if (phase < 1.0e-6f)
                length = pending;

            phase += 1.0f / length;
            if (phase >= 1.0f) { phase -= 1.0f; length = pending; }

            auto p2 = phase + 0.5f;
            if (p2 >= 1.0f) p2 -= 1.0f;

            // Delay grows by two samples per sample, so each head reads backwards.
            const auto pi = juce::MathConstants<float>::pi;
            const auto w1 = std::sin (pi * phase), w2 = std::sin (pi * p2);
            const auto y = w1 * w1 * line.read (2.0f + 2.0f * phase * length)
                         + w2 * w2 * line.read (2.0f + 2.0f * p2 * length);
            line.write (x);
            return y;
        }

    private:
        double sampleRate = 48000.0;
        FracDelay line;
        float phase = 0.0f, length = 24000.0f, pending = 24000.0f;
    };

    /** Two slowly modulated short delays panned left and right: a double-tracked
        vocal. Returns only the doubled voices; add them to the dry signal. */
    class Doubler
    {
    public:
        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            lineL.prepare ((int) (0.04 * sampleRate));
            lineR.prepare ((int) (0.04 * sampleRate));
            reset();
        }

        void reset() noexcept { lineL.reset(); lineR.reset(); phaseL = 0.0; phaseR = 1.3; }

        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            const auto twoPi = juce::MathConstants<double>::twoPi;
            const auto ms = (float) (0.001 * sampleRate);
            const auto dL = ms * (12.0f + 1.6f * (float) std::sin (phaseL));
            const auto dR = ms * (18.5f + 2.1f * (float) std::sin (phaseR));
            phaseL += twoPi * 0.29 / sampleRate; if (phaseL > twoPi) phaseL -= twoPi;
            phaseR += twoPi * 0.41 / sampleRate; if (phaseR > twoPi) phaseR -= twoPi;

            outL = lineL.read (dL);
            outR = lineR.read (dR);
            const auto mono = 0.5f * (inL + inR);
            lineL.write (mono);
            lineR.write (mono);
        }

    private:
        double sampleRate = 48000.0;
        FracDelay lineL, lineR;
        double phaseL = 0.0, phaseR = 1.3;
    };

    /** Resonant low-pass on the wet signal whose cutoff is swept by an LFO or by
        the level of the vocal. Depth 0 leaves the signal untouched. */
    class SweepFilter
    {
    public:
        enum class Mode { lfo = 0, envelope };

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            envAttack  = smoothingCoeff (0.01, sampleRate);
            envRelease = smoothingCoeff (0.25, sampleRate);
            depthGlide = smoothingCoeff (0.05, sampleRate);
            reset();
        }

        void reset() noexcept
        {
            for (auto& s : state) s = { 0.0f, 0.0f };
            phase = 0.0; envelope = 0.0f; counter = 0;
            depth = targetDepth;
        }

        void setParameters (Mode newMode, float rateHz, float depth01) noexcept
        {
            mode = newMode;
            rate = rateHz;
            targetDepth = juce::jlimit (0.0f, 1.0f, depth01);
        }

        /** Last cutoff in Hz, for the editor. */
        float currentCutoff() const noexcept { return cutoff; }

        void process (float& l, float& r, float detectorAbs) noexcept
        {
            depth += (targetDepth - depth) * depthGlide;
            envelope += (detectorAbs - envelope) * (detectorAbs > envelope ? envAttack : envRelease);

            const auto twoPi = juce::MathConstants<double>::twoPi;
            phase += twoPi * rate / sampleRate;
            if (phase > twoPi) phase -= twoPi;

            if (depth < 1.0e-4f)
                return;

            // Recompute the coefficients every 8 samples; tan() is the costly part.
            if (counter-- <= 0)
            {
                counter = 7;
                float closed;   // 0 = fully open, 1 = fully closed
                if (mode == Mode::lfo)
                {
                    closed = 0.5f + 0.5f * (float) std::sin (phase);
                }
                else
                {
                    const auto db = juce::Decibels::gainToDecibels (envelope, -100.0f);
                    closed = 1.0f - juce::jlimit (0.0f, 1.0f, (db + 50.0f) / 40.0f);
                }

                const auto top = juce::jmin (16000.0f, (float) (sampleRate * 0.4));
                cutoff = top * std::pow (2.0f, -6.0f * depth * closed);
                g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
                k = 1.4142f - 1.0f * depth;   // more resonance as the sweep gets deeper
                a1 = 1.0f / (1.0f + g * (g + k));
            }

            l = tick (l, state[0]);
            r = tick (r, state[1]);
        }

    private:
        struct State { float ic1, ic2; };

        float tick (float x, State& s) noexcept
        {
            // Topology-preserving state-variable filter (Simper), low-pass output.
            const auto v1 = a1 * (s.ic1 + g * (x - s.ic2));
            const auto v2 = s.ic2 + g * v1;
            s.ic1 = 2.0f * v1 - s.ic1;
            s.ic2 = 2.0f * v2 - s.ic2;
            return v2;
        }

        double sampleRate = 48000.0, phase = 0.0;
        Mode mode = Mode::lfo;
        float rate = 0.5f, depth = 0.0f, targetDepth = 0.0f, depthGlide = 0.001f;
        float envelope = 0.0f, envAttack = 0.1f, envRelease = 0.01f;
        float cutoff = 16000.0f, g = 1.0f, k = 1.4142f, a1 = 0.3f;
        int counter = 0;
        std::array<State, 2> state {};
    };

    /** Gate keyed by the vocal: open while the vocal is above the threshold, held
        for a while after it drops, then closed quickly. Returns a gain 0..1. */
    class TailGate
    {
    public:
        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            envAttack  = smoothingCoeff (0.001, sampleRate);
            envRelease = smoothingCoeff (0.050, sampleRate);
            open       = smoothingCoeff (0.002, sampleRate);
            close      = smoothingCoeff (0.025, sampleRate);
            reset();
        }

        void reset() noexcept { envelope = 0.0f; gain = 1.0f; holdLeft = 0; }

        void setParameters (float thresholdDb, float holdMs) noexcept
        {
            threshold = juce::Decibels::decibelsToGain (thresholdDb);
            holdSamples = (int) (holdMs * 0.001 * sampleRate);
        }

        float process (float detectorAbs) noexcept
        {
            envelope += (detectorAbs - envelope) * (detectorAbs > envelope ? envAttack : envRelease);
            if (envelope > threshold)
                holdLeft = holdSamples;
            else if (holdLeft > 0)
                --holdLeft;

            const auto target = holdLeft > 0 ? 1.0f : 0.0f;
            gain += (target - gain) * (target > gain ? open : close);
            return gain;
        }

    private:
        double sampleRate = 48000.0;
        float envelope = 0.0f, gain = 1.0f, threshold = 0.02f;
        float envAttack = 0.1f, envRelease = 0.01f, open = 0.1f, close = 0.01f;
        int holdSamples = 12000, holdLeft = 0;
    };
}
