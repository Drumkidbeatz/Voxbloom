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
        p.setCurrentProgram (1);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
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
