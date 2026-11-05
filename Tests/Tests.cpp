// Headless checks for VoxBloom, plus optional rendering of audio demos.
//   VoxBloomTests                 run the checks
//   VoxBloomTests --render <dir>  also write demo WAVs (dry vs. presets)

#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "Params.h"

namespace
{
    constexpr double fs = 48000.0;
    constexpr int block = 512;
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! ok) ++failures;
    }

    void setParam (VoxBloomProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        jassert (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void neutral (VoxBloomProcessor& p)
    {
        using namespace vb::pid;
        setParam (p, delaySync, 0);  setParam (p, delayTime, 250); setParam (p, delayMode, 0);
        setParam (p, delayFeedback, 0); setParam (p, delayLowCut, 20); setParam (p, delayHighCut, 20000);
        setParam (p, delayDrive, 0); setParam (p, delayWobble, 0); setParam (p, delayLevel, 100);
        setParam (p, reverbLevel, 0); setParam (p, reverbSource, 0);
        setParam (p, duckAmount, 0); setParam (p, width, 100); setParam (p, mix, 50); setParam (p, output, 0);
    }

    juce::AudioBuffer<float> run (VoxBloomProcessor& p, const juce::AudioBuffer<float>& input, int blockSize = block)
    {
        juce::AudioBuffer<float> out (input);
        juce::MidiBuffer midi;
        for (int start = 0; start < out.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, out.getNumSamples() - start);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), start, n);
            p.processBlock (view, midi);
        }
        return out;
    }

    juce::AudioBuffer<float> impulse (double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * fs));
        b.clear();
        b.setSample (0, 0, 1.0f);
        b.setSample (1, 0, 1.0f);
        return b;
    }

    int peakIndex (const juce::AudioBuffer<float>& b, int ch, int from, int to)
    {
        int best = from; float bestV = 0.0f;
        for (int i = from; i < juce::jmin (to, b.getNumSamples()); ++i)
            if (std::abs (b.getSample (ch, i)) > bestV) { bestV = std::abs (b.getSample (ch, i)); best = i; }
        return best;
    }

    float rms (const juce::AudioBuffer<float>& b, int from, int to)
    {
        double s = 0.0; int n = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = from; i < juce::jmin (to, b.getNumSamples()); ++i, ++n)
                s += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return n > 0 ? (float) std::sqrt (s / n) : 0.0f;
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i))) return false;
        return true;
    }

    juce::AudioBuffer<float> section (const juce::AudioBuffer<float>& b, int from, int to)
    {
        juce::AudioBuffer<float> out (b.getNumChannels(), to - from);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            out.copyFrom (ch, 0, b, ch, from, to - from);
        return out;
    }

    juce::AudioBuffer<float> noiseBurst (double seconds, double burstSeconds, float level = 0.25f)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * fs));
        b.clear();
        juce::Random rng (3);
        for (int i = 0; i < (int) (burstSeconds * fs); ++i)
        {
            const auto v = level * (rng.nextFloat() * 2.0f - 1.0f);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    juce::AudioBuffer<float> toneBurst (double seconds, double burstSeconds, double hz, float level = 0.3f)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * fs));
        b.clear();
        for (int i = 0; i < (int) (burstSeconds * fs); ++i)
        {
            const auto env = juce::jmin (1.0, i / (0.005 * fs), (burstSeconds * fs - i) / (0.005 * fs));
            const auto v = level * (float) (env * std::sin (juce::MathConstants<double>::twoPi * hz * i / fs));
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    /** Zero-crossing estimate of the dominant frequency in a section. */
    double zeroCrossingHz (const juce::AudioBuffer<float>& b, int ch, int from, int to)
    {
        int crossings = 0;
        for (int i = from + 1; i < to; ++i)
            if ((b.getSample (ch, i - 1) < 0.0f) != (b.getSample (ch, i) < 0.0f))
                ++crossings;
        return crossings * 0.5 * fs / (to - from);
    }

    /** Energy-weighted mean time in seconds. */
    double energyCentroid (const juce::AudioBuffer<float>& b, int from, int to)
    {
        double sum = 0.0, weighted = 0.0;
        for (int i = from; i < to; ++i)
        {
            const auto e = (double) b.getSample (0, i) * b.getSample (0, i) + (double) b.getSample (1, i) * b.getSample (1, i);
            sum += e;
            weighted += e * (i - from);
        }
        return sum > 0.0 ? weighted / sum / fs : 0.0;
    }

    /** High-frequency share: energy of the first difference relative to the signal. */
    float brightness (const juce::AudioBuffer<float>& b, int from, int to)
    {
        double hf = 0.0, all = 0.0;
        for (int i = from + 1; i < to; ++i)
        {
            const auto d = b.getSample (0, i) - b.getSample (0, i - 1);
            hf += (double) d * d;
            all += (double) b.getSample (0, i) * b.getSample (0, i);
        }
        return all > 0.0 ? (float) (hf / all) : 0.0f;
    }

    std::unique_ptr<VoxBloomProcessor> makeProcessor (std::function<void (VoxBloomProcessor&)> setup = {})
    {
        auto p = std::make_unique<VoxBloomProcessor>();
        p->setPlayConfigDetails (2, 2, fs, block);
        if (setup) setup (*p);
        p->prepareToPlay (fs, block);
        return p;
    }

    /** A rough "sung" phrase: harmonic tone with vowel formants, syllables and gaps. */
    juce::AudioBuffer<float> vocalPhrase (double sampleRate)
    {
        struct Note { double start, length, midi; int vowel; };
        const Note notes[] { { 0.00, 0.30, 62, 0 }, { 0.32, 0.30, 65, 1 }, { 0.64, 0.55, 67, 2 },
                             { 1.80, 0.25, 65, 1 }, { 2.08, 0.25, 64, 0 }, { 2.36, 0.90, 62, 3 },
                             { 4.20, 0.35, 69, 2 }, { 4.60, 1.10, 67, 1 } };
        static constexpr double formants[4][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 },
                                                 { 300, 2200, 3000 }, { 570, 840, 2410 } };
        const auto total = (int) (9.0 * sampleRate);
        juce::AudioBuffer<float> b (2, total);
        b.clear();
        juce::Random rng (7);

        for (const auto& n : notes)
        {
            const auto f0 = 440.0 * std::pow (2.0, (n.midi - 69.0) / 12.0);
            const auto s0 = (int) (n.start * sampleRate), len = (int) (n.length * sampleRate);
            double phase = 0.0;
            for (int i = 0; i < len && s0 + i < total; ++i)
            {
                const auto t = i / sampleRate;
                const auto env = juce::jmin (1.0, t / 0.03) * juce::jmin (1.0, (n.length - t) / 0.08);
                const auto vib = 1.0 + 0.006 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * t) * juce::jmin (1.0, t / 0.2);
                phase += juce::MathConstants<double>::twoPi * f0 * vib / sampleRate;
                double s = 0.0;
                for (int h = 1; h * f0 < 6000.0; ++h)
                {
                    double w = 0.0;
                    for (auto fm : formants[n.vowel])
                        w += 1.0 / (1.0 + std::pow ((h * f0 - fm) / 120.0, 2.0));
                    s += (0.15 + w) * std::sin (h * phase) / h;
                }
                s += 0.01 * (rng.nextFloat() * 2.0f - 1.0f);
                const auto v = (float) (0.22 * env * s);
                b.addSample (0, s0 + i, v);
                b.addSample (1, s0 + i, v);
            }
        }
        return b;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& b, double sampleRate)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        if (stream == nullptr) return;
        auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                       .withNumChannels (b.getNumChannels())
                                                       .withBitsPerSample (24);
        std::unique_ptr<juce::OutputStream> os (stream.release());
        if (auto writer = wav.createWriterFor (os, options))
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    using namespace vb;

    std::cout << "Echo timing\n";
    {
        auto p = makeProcessor (neutral);
        auto out = run (*p, impulse (1.0));
        const auto idx = peakIndex (out, 0, 100, out.getNumSamples());
        check (std::abs (idx - 12000) <= 2, "250 ms delay echoes at sample " + juce::String (idx) + " (expected 12000)");
    }
    {
        // 1 kHz burst: the first echo should come back at the input level (100 % level, 50 % mix).
        auto p = makeProcessor (neutral);
        juce::AudioBuffer<float> burst (2, (int) fs);
        burst.clear();
        for (int i = 0; i < (int) (0.05 * fs); ++i)
            for (int ch = 0; ch < 2; ++ch)
                burst.setSample (ch, i, 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs));
        auto out = run (*p, burst);
        const auto ratioDb = juce::Decibels::gainToDecibels (rms (out, 12000 + 480, 12000 + 1920) / rms (burst, 480, 1920));
        check (std::abs (ratioDb) < 0.5f, "echo returns at input level: " + juce::String (ratioDb, 2) + " dB");
    }
    {
        auto p = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::delaySync, 1); setParam (q, pid::delayNote, 8); });
        auto out = run (*p, impulse (1.0));
        const auto idx = peakIndex (out, 0, 100, out.getNumSamples());
        check (std::abs (idx - 24000) <= 2, "1/4 note at 120 BPM echoes at " + juce::String (idx) + " (expected 24000)");
    }
    {
        auto p = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::delayMode, 1); setParam (q, pid::delayFeedback, 50); });
        auto out = run (*p, impulse (1.0));
        check (std::abs (out.getSample (0, 12000)) > 0.3f && std::abs (out.getSample (1, 12000)) < 0.01f, "ping-pong: 1st echo left only");
        check (std::abs (out.getSample (1, 24000)) > 0.1f && std::abs (out.getSample (0, 24000)) < 0.01f, "ping-pong: 2nd echo right only");
    }

    std::cout << "Reverb tail\n";
    {
        auto p = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::delayLevel, 0); setParam (q, pid::reverbLevel, 100);
                                              setParam (q, pid::reverbDecay, 3.0f); setParam (q, pid::mix, 100); });
        auto out = run (*p, impulse (6.0));
        const auto early = rms (out, (int) (0.1 * fs), (int) (0.6 * fs));
        const auto late  = rms (out, (int) (3.1 * fs), (int) (3.6 * fs));
        const auto dropDb = juce::Decibels::gainToDecibels (late / early);
        check (allFinite (out), "output finite");
        check (early > 1.0e-4f, "reverb produces a tail (rms " + juce::String (early, 5) + ")");
        check (dropDb < -40.0f && dropDb > -80.0f, "3 s decay drops ~60 dB after 3 s: " + juce::String (dropDb, 1) + " dB");
        const auto stereoDiff = rms (out, 4800, 48000);
        juce::ignoreUnused (stereoDiff);
        double corr = 0, l2 = 0, r2 = 0;
        for (int i = 4800; i < 48000; ++i) { auto l = out.getSample (0, i), r = out.getSample (1, i); corr += l * r; l2 += l * l; r2 += r * r; }
        check (std::abs (corr / std::sqrt (l2 * r2)) < 0.5, "reverb is decorrelated L/R (corr " + juce::String (corr / std::sqrt (l2 * r2), 2) + ")");
    }

    std::cout << "Ducking\n";
    {
        juce::AudioBuffer<float> tone (2, (int) (4.0 * fs));
        tone.clear();
        for (int i = 0; i < (int) (2.0 * fs); ++i)
            for (int ch = 0; ch < 2; ++ch)
                tone.setSample (ch, i, 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 330.0 * i / fs));

        auto make = [] (float duck) { return makeProcessor ([duck] (auto& q) { neutral (q); setParam (q, pid::mix, 100);
                                                     setParam (q, pid::delayFeedback, 50); setParam (q, pid::duckAmount, duck); }); };
        auto a = make (0);   auto outA = run (*a, tone);
        auto b = make (100); auto outB = run (*b, tone);
        const auto during0 = rms (outA, (int) (1.0 * fs), (int) (1.9 * fs));
        const auto during1 = rms (outB, (int) (1.0 * fs), (int) (1.9 * fs));
        const auto after1  = rms (outB, (int) (2.6 * fs), (int) (3.0 * fs));
        check (during1 < 0.1f * during0, "wet ducked while the vocal plays (" + juce::String (juce::Decibels::gainToDecibels (during1 / during0), 1) + " dB)");
        check (after1 > 4.0f * during1, "tail blooms after the vocal stops");
    }

    std::cout << "Freeze\n";
    {
        auto setup = [] (VoxBloomProcessor& q) { neutral (q); setParam (q, pid::mix, 100); setParam (q, pid::delayFeedback, 40);
                                                 setParam (q, pid::reverbLevel, 100); setParam (q, pid::reverbDecay, 1.5f); };
        auto in = noiseBurst (8.0, 0.4);
        // A second burst arrives while frozen; it must not enter the frozen tail.
        for (int i = (int) (4.0 * fs); i < (int) (4.4 * fs); ++i)
            for (int ch = 0; ch < 2; ++ch)
                in.setSample (ch, i, 0.25f * std::sin ((float) i * 0.37f));

        auto p = makeProcessor (setup);
        auto first = run (*p, section (in, 0, (int) (0.6 * fs)));
        setParam (*p, pid::freeze, 1);
        auto frozen = run (*p, section (in, (int) (0.6 * fs), (int) (8.0 * fs)));
        const auto early = rms (frozen, (int) (0.5 * fs), (int) (1.5 * fs));
        const auto late  = rms (frozen, (int) (6.0 * fs), (int) (7.0 * fs));
        check (allFinite (frozen) && late > 0.5f * early && late < 2.0f * early,
               "frozen tail holds its level (" + juce::String (juce::Decibels::gainToDecibels (late / early), 1) + " dB after 6 s)");

        // Same run without the second burst: the frozen tail must match (dry is off at 100 % mix).
        auto quiet = noiseBurst (8.0, 0.4);
        auto q = makeProcessor (setup);
        run (*q, section (quiet, 0, (int) (0.6 * fs)));
        setParam (*q, pid::freeze, 1);
        auto frozenQuiet = run (*q, section (quiet, (int) (0.6 * fs), (int) (8.0 * fs)));
        float maxDiff = 0.0f;
        for (int i = (int) (3.4 * fs); i < frozen.getNumSamples(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (frozen.getSample (0, i) - frozenQuiet.getSample (0, i)));
        check (maxDiff < 1.0e-3f, "new input does not enter while frozen (diff " + juce::String (maxDiff, 6) + ")");

        setParam (*p, pid::freeze, 0);
        auto released = run (*p, juce::AudioBuffer<float> (2, (int) (6.0 * fs)) );
        juce::ignoreUnused (first);
        check (rms (released, (int) (5.0 * fs), (int) (6.0 * fs)) < 0.01f * late, "tail dies away after unfreezing");
    }

    std::cout << "Throw\n";
    {
        auto in = toneBurst (2.0, 0.2, 440.0);
        auto setup = [] (VoxBloomProcessor& q) { neutral (q); setParam (q, pid::throwMode, 1); };
        auto p = makeProcessor (setup);
        auto out = run (*p, in);
        check (rms (out, 12000, 12000 + 9600) < 1.0e-4f, "throw mode, not pressed: no echo");
        check (std::abs (rms (out, 0, 9600) - rms (in, 0, 9600)) < 1.0e-4f, "throw mode, not pressed: vocal unchanged");

        auto q = makeProcessor ([setup] (VoxBloomProcessor& r) { setup (r); setParam (r, pid::throwActive, 1); });
        auto thrown = run (*q, in);
        check (rms (thrown, 12000 + 480, 12000 + 9000) > 0.5f * rms (in, 480, 9000), "throw pressed: the word is echoed");

        juce::MemoryBlock state;
        q->getStateInformation (state);
        auto r = makeProcessor();
        r->setStateInformation (state.getData(), (int) state.getSize());
        check (r->apvts.getRawParameterValue (pid::throwActive)->load() < 0.5f
                   && r->apvts.getRawParameterValue (pid::throwMode)->load() > 0.5f,
               "a session does not reopen with Throw held down");
    }

    std::cout << "Shimmer and Lo-Fi\n";
    {
        auto in = toneBurst (1.5, 0.15, 500.0);
        auto p = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::delayFeedback, 80);
                                              setParam (q, pid::shimmerAmount, 100); setParam (q, pid::shimmerPitch, 5); });
        auto out = run (*p, in);
        const auto f1 = zeroCrossingHz (out, 0, 12000 + 1500, 12000 + 6000);
        const auto f2 = zeroCrossingHz (out, 0, 24000 + 1500, 24000 + 6000);
        check (std::abs (f1 - 500.0) < 30.0, "1st echo keeps the pitch (" + juce::String (f1, 0) + " Hz)");
        check (std::abs (f2 - 1000.0) < 60.0, "2nd echo is an octave up (" + juce::String (f2, 0) + " Hz)");
        check (rms (out, 24000 + 600, 24000 + 6600) > 0.3f * rms (out, 12000 + 600, 12000 + 6600),
               "shifted echo lands on the beat");

        auto l = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::lofi, 100); });
        auto crushed = run (*l, in);
        const auto step = 2.0f / 16.0f;
        float worst = 0.0f;
        for (int i = 12000; i < 12000 + 7200; ++i)
        {
            const auto v = crushed.getSample (0, i);
            worst = juce::jmax (worst, std::abs (v - std::round (v / step) * step));
        }
        check (worst < 1.0e-3f, "lo-fi 100 %: echo quantised to 4 bits (off by " + juce::String (worst, 5) + ")");
        check (allFinite (crushed), "lo-fi output finite");
    }

    std::cout << "Reverse, gate and sweep\n";
    {
        auto setup = [] (float rev) { return [rev] (VoxBloomProcessor& q) { neutral (q); setParam (q, pid::delayLevel, 0); setParam (q, pid::mix, 100);
                                                                            setParam (q, pid::reverbLevel, 100); setParam (q, pid::reverbDecay, 0.8f);
                                                                            setParam (q, pid::reverbPreDelay, 0); setParam (q, pid::reverse, rev);
                                                                            setParam (q, pid::reverseTime, 600); }; };
        auto in = noiseBurst (2.0, 0.02);
        auto a = makeProcessor (setup (0));   auto normal   = run (*a, in);
        auto b = makeProcessor (setup (100)); auto reversed = run (*b, in);
        const auto c0 = energyCentroid (normal, 0, (int) (1.2 * fs));
        const auto c1 = energyCentroid (reversed, 0, (int) (1.2 * fs));
        check (c1 > c0 + 0.15, "reverse: energy swells later (" + juce::String (c0, 2) + " s -> " + juce::String (c1, 2) + " s)");

        auto tone = toneBurst (3.0, 0.5, 330.0);
        auto gateSetup = [] (float amount) { return [amount] (VoxBloomProcessor& q) { neutral (q); setParam (q, pid::delayLevel, 0); setParam (q, pid::mix, 100);
                                                                                     setParam (q, pid::reverbLevel, 100); setParam (q, pid::reverbDecay, 4.0f);
                                                                                     setParam (q, pid::gateAmount, amount); setParam (q, pid::gateHold, 100); }; };
        auto g0 = makeProcessor (gateSetup (0));   auto open   = run (*g0, tone);
        auto g1 = makeProcessor (gateSetup (100)); auto gated  = run (*g1, tone);
        const auto during = rms (gated, (int) (0.2 * fs), (int) (0.45 * fs)) / rms (open, (int) (0.2 * fs), (int) (0.45 * fs));
        const auto after  = rms (gated, (int) (1.0 * fs), (int) (1.5 * fs)) / rms (open, (int) (1.0 * fs), (int) (1.5 * fs));
        check (during > 0.9f, "gate open while the vocal plays");
        check (after < 0.01f, "gate cuts the tail after the hold (" + juce::String (juce::Decibels::gainToDecibels (after), 0) + " dB)");

        auto noise = noiseBurst (1.0, 0.2);
        auto s0 = makeProcessor ([] (auto& q) { neutral (q); });
        auto s1 = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::sweepDepth, 100); setParam (q, pid::sweepRate, 0.05f); });
        auto plain = run (*s0, noise), swept = run (*s1, noise);
        const auto b0 = brightness (plain, 12000 + 500, 12000 + 9000), b1 = brightness (swept, 12000 + 500, 12000 + 9000);
        check (b1 < 0.3f * b0, "sweep filter darkens the echo (" + juce::String (b0, 3) + " -> " + juce::String (b1, 3) + ")");
        check (std::abs (rms (plain, 0, 9600) - rms (swept, 0, 9600)) < 1.0e-7f, "sweep leaves the dry vocal alone");

        auto e1 = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::sweepDepth, 100); setParam (q, pid::sweepMode, 1); });
        check (allFinite (run (*e1, vocalPhrase (fs))), "envelope sweep output finite");
    }

    std::cout << "Doubler\n";
    {
        auto in = vocalPhrase (fs);
        auto a = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::mix, 0); });
        auto b = makeProcessor ([] (auto& q) { neutral (q); setParam (q, pid::mix, 0); setParam (q, pid::doubler, 100); });
        auto plain = run (*a, in), doubled = run (*b, in);
        double side0 = 0, side1 = 0;
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            side0 += std::pow (plain.getSample (0, i) - plain.getSample (1, i), 2.0f);
            side1 += std::pow (doubled.getSample (0, i) - doubled.getSample (1, i), 2.0f);
        }
        check (side0 < 1.0e-12 && side1 > 0.01 * rms (in, 0, in.getNumSamples()), "doubler turns a mono vocal into stereo");
        const auto gainDb = juce::Decibels::gainToDecibels (rms (doubled, 0, in.getNumSamples()) / rms (plain, 0, in.getNumSamples()));
        check (gainDb > 0.5f && gainDb < 4.5f, "doubler adds a moderate amount of level (+" + juce::String (gainDb, 1) + " dB)");
    }

    std::cout << "A/B\n";
    {
        auto p = makeProcessor();
        setParam (*p, pid::mix, 20);
        p->switchToSlot (1);
        check (std::abs (p->apvts.getRawParameterValue (pid::mix)->load() - 20.0f) < 0.01f, "unused slot B starts as a copy of A");
        setParam (*p, pid::mix, 80);
        p->switchToSlot (0);
        const auto mixA = p->apvts.getRawParameterValue (pid::mix)->load();
        p->switchToSlot (1);
        const auto mixB = p->apvts.getRawParameterValue (pid::mix)->load();
        check (std::abs (mixA - 20.0f) < 0.01f && std::abs (mixB - 80.0f) < 0.01f, "A and B keep their own settings");

        p->setEditorScale (1.25f);
        juce::MemoryBlock state;
        p->getStateInformation (state);
        auto q = makeProcessor();
        q->setStateInformation (state.getData(), (int) state.getSize());
        const auto activeOk = q->getActiveSlot() == 1 && std::abs (q->apvts.getRawParameterValue (pid::mix)->load() - 80.0f) < 0.01f;
        q->switchToSlot (0);
        check (activeOk && std::abs (q->apvts.getRawParameterValue (pid::mix)->load() - 20.0f) < 0.01f, "A/B survives save/load");
        check (std::abs (q->getEditorScale() - 1.25f) < 1.0e-4f, "window size survives save/load");
        check (! q->apvts.state.getChildWithName ("VoxBloomAB").isValid(), "A/B data stays out of the parameter tree");

        p->copyActiveSlotToOther();
        p->switchToSlot (0);
        check (std::abs (p->apvts.getRawParameterValue (pid::mix)->load() - 80.0f) < 0.01f, "copy B to A");
    }

    std::cout << "Factory presets\n";
    {
        juce::AudioBuffer<float> burst (2, (int) (10.0 * fs));
        burst.clear();
        juce::Random rng (1);
        for (int i = 0; i < (int) (1.5 * fs); ++i)
            for (int ch = 0; ch < 2; ++ch)
                burst.setSample (ch, i, 0.25f * (rng.nextFloat() * 2.0f - 1.0f));

        const auto& presets = factoryPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            auto p = makeProcessor ([i] (auto& q) { q.setCurrentProgram (i); });
            auto out = run (*p, burst);
            const auto peak = out.getMagnitude (0, out.getNumSamples());
            const auto tail = rms (out, (int) (2.0 * fs), (int) (3.0 * fs));
            const auto end  = rms (out, (int) (9.0 * fs), (int) (10.0 * fs));
            check (allFinite (out) && peak < 2.0f && end <= tail * 1.01f,
                   juce::String (presets[(size_t) i].name).paddedRight (' ', 18)
                       + " peak " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS"
                       + ", tail " + juce::String (juce::Decibels::gainToDecibels (tail), 1)
                       + " -> " + juce::String (juce::Decibels::gainToDecibels (end, -120.0f), 1) + " dB");
        }
    }

    std::cout << "State and presets\n";
    {
        auto a = makeProcessor ([] (auto& q) { q.setCurrentProgram (3); setParam (q, pid::mix, 77); });
        juce::MemoryBlock state;
        a->getStateInformation (state);
        auto b = makeProcessor();
        b->setStateInformation (state.getData(), (int) state.getSize());
        bool same = true;
        for (auto* param : a->getParameters())
        {
            auto* r = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (std::abs (r->getValue() - b->apvts.getParameter (r->getParameterID())->getValue()) > 1.0e-6f) same = false;
        }
        check (same, "all parameters survive save/load");
        check (b->getCurrentPresetName() == factoryPresets()[3].name, "preset name survives: " + b->getCurrentPresetName());

        auto tmpName = "zz-test-" + juce::String (juce::Random::getSystemRandom().nextInt (100000));
        check (a->saveUserPreset (tmpName), "user preset saved");
        auto file = UserPresets::folder().getChildFile (tmpName + UserPresets::extension);
        auto c = makeProcessor();
        check (c->loadUserPreset (file) && std::abs (c->apvts.getRawParameterValue (pid::mix)->load() - 77.0f) < 0.01f, "user preset loads back");
        file.deleteFile();
    }

    std::cout << "Block sizes and layouts\n";
    {
        auto in = vocalPhrase (fs);
        auto setup = [] (VoxBloomProcessor& p) { p.setCurrentProgram (1); };
        auto a = makeProcessor (setup); auto outA = run (*a, in, 512);
        auto b = makeProcessor (setup); auto outB = run (*b, in, 4096);
        auto c = makeProcessor (setup); auto outC = run (*c, in, 37);
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < in.getNumSamples(); ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (outA.getSample (ch, i) - outB.getSample (ch, i)),
                                      std::abs (outA.getSample (ch, i) - outC.getSample (ch, i)));
        check (maxDiff < 0.02f, "blocks of 37/512/4096 give the same result (max diff " + juce::String (maxDiff, 5) + ")");

        VoxBloomProcessor mono;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::mono());
        check (mono.setBusesLayout (layout), "mono layout accepted");
        mono.prepareToPlay (fs, block);
        juce::AudioBuffer<float> monoIn (1, (int) fs);
        monoIn.clear(); monoIn.setSample (0, 0, 1.0f);
        auto monoOut = run (mono, monoIn);
        check (allFinite (monoOut) && monoOut.getMagnitude (0, monoOut.getNumSamples()) < 2.0f, "mono processing is stable");
    }

    std::cout << "Performance\n";
    {
        auto p = makeProcessor ([] (auto& q) { q.setCurrentProgram (5); });
        auto in = vocalPhrase (fs);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (*p, in);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const auto audioMs = 1000.0 * in.getNumSamples() / fs;
        std::cout << "  INFO  " << juce::String (audioMs / ms, 0) << "x realtime (stereo, 48 kHz, one core, this build type)\n";
    }
    {
        // Everything switched on at once: the worst case for CPU.
        auto p = makeProcessor ([] (auto& q) { q.setCurrentProgram (5);
                                              for (auto* id : { &pid::shimmerAmount, &pid::lofi, &pid::reverse, &pid::sweepDepth,
                                                                &pid::gateAmount, &pid::doubler })
                                                  setParam (q, *id, 50); });
        auto in = vocalPhrase (fs);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = run (*p, in);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const auto audioMs = 1000.0 * in.getNumSamples() / fs;
        check (allFinite (out) && out.getMagnitude (0, out.getNumSamples()) < 2.0f, "all features at once: stable output");
        std::cout << "  INFO  " << juce::String (audioMs / ms, 0) << "x realtime with every feature on\n";
    }

    if (argc >= 3 && juce::String (argv[1]) == "--render")
    {
        const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        dir.createDirectory();
        const double sr = 44100.0;
        auto dry = vocalPhrase (sr);
        writeWav (dir.getChildFile ("00 Dry.wav"), dry, sr);
        for (int i = 1; i < (int) factoryPresets().size(); ++i)
        {
            VoxBloomProcessor p;
            p.setPlayConfigDetails (2, 2, sr, block);
            p.setCurrentProgram (i);
            p.prepareToPlay (sr, block);
            auto out = run (p, dry);
            const auto name = juce::String (i).paddedLeft ('0', 2) + " " + factoryPresets()[(size_t) i].name + ".wav";
            writeWav (dir.getChildFile (juce::File::createLegalFileName (name)), out, sr);
        }
        std::cout << "Rendered demos to " << dir.getFullPathName() << "\n";
    }

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
    {
        VoxBloomProcessor p;
        p.setPlayConfigDetails (2, 2, fs, block);
        p.prepareToPlay (fs, block);
        p.setCurrentProgram (5);
        // Feed a phrase so the scope has something to show; stop mid-tail.
        auto phrase = vocalPhrase (fs);
        run (p, section (phrase, 0, (int) (3.0 * fs)));
        // Show the performance controls in action: freeze the tail as the phrase ends.
        setParam (p, pid::freeze, 1);
        setParam (p, pid::sweepDepth, 40);
        run (p, section (phrase, (int) (3.0 * fs), (int) (4.2 * fs)));
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
        std::cout << "Editor size " << editor->getWidth() << " x " << editor->getHeight() << "\n";
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        out.deleteFile();
        juce::PNGImageFormat png;
        if (auto stream = out.createOutputStream())
            png.writeImageToStream (image, *stream);
        editor.reset();
        std::cout << "Snapshot written to " << out.getFullPathName() << "\n";
        return 0;
    }

    std::cout << (failures == 0 ? "\nAll checks passed\n" : "\n" + juce::String (failures) + " check(s) FAILED\n");
    return failures == 0 ? 0 : 1;
}
