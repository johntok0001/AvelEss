// Offline render test for Avel Ess: feeds a synthetic vocal through the processor, drives the editor,
// writes UI screenshots to ./shots and checks presets, state round trip and undo.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <random>

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

// "sa - she - sis - so": vowels with formants + noisy "s" bursts
struct VocalSynth
{
    double sr = 48000, t = 0, phase = 0;
    std::mt19937 rng { 42 };
    std::normal_distribution<float> nd { 0.0f, 1.0f };
    juce::dsp::IIR::Filter<float> f1, f2, f3, sib1, sib2;

    void prepare (double s)
    {
        sr = s;
        juce::dsp::ProcessSpec spec { sr, 512, 1 };
        for (auto* f : { &f1, &f2, &f3, &sib1, &sib2 }) f->prepare (spec);
        f1.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 700, 4);
        f2.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 1200, 5);
        f3.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 2600, 6);
        sib1.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 4800, 0.7f);
        sib2.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 7500, 1.2f, 3.0f);
    }

    float next()
    {
        const double cycle = 0.62, pos = std::fmod (t, cycle);
        const int syl = (int) (t / cycle) % 4;
        float out = 0.0f;
        // vowel 0.10 .. 0.48 s, with vibrato and a soft envelope
        if (pos > 0.10 && pos < 0.48)
        {
            const double e = std::sin (juce::MathConstants<double>::pi * (pos - 0.10) / 0.38);
            const double f0 = 190.0 + 25.0 * syl + 3.0 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t);
            phase += f0 / sr; if (phase >= 1) phase -= 1;
            const float saw = (float) (2.0 * phase - 1.0);
            const float v = f1.processSample (saw) * 1.0f + f2.processSample (saw) * 0.6f + f3.processSample (saw) * 0.35f;
            out += (float) (v * e * 0.9);
        }
        // "s" at the start of the syllable (0.0 .. 0.11 s) and on "sis" also at the end
        auto burst = [&] (double a, double len, float g)
        {
            if (pos > a && pos < a + len)
            {
                const double e = std::pow (std::sin (juce::MathConstants<double>::pi * (pos - a) / len), 0.5);
                out += sib2.processSample (sib1.processSample (nd (rng))) * (float) e * g;
            }
        };
        burst (0.0, 0.11, syl == 1 ? 0.10f : 0.17f);
        if (syl == 2) burst (0.48, 0.12, 0.2f);
        t += 1.0 / sr;
        return out;
    }
};

static void runAudio (AvelEssProcessor& p, VocalSynth& v, int blocks, AvelEssEditor* ed)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i) { const float x = v.next(); buf.setSample (0, i, x); buf.setSample (1, i, x * 0.92f); }
        p.processBlock (buf, midi);
        if (ed != nullptr && (b % 2) == 1) ed->pumpForTest();
    }
}

static void snapshot (juce::Component& c, const juce::String& name)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("shots").getChildFile (name + ".png");
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("  wrote %s\n", f.getFullPathName().toRawUTF8());
}

static void settle() { juce::MessageManager::getInstance()->runDispatchLoopUntil (300); }

static void setParam (AvelEssProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("Avel Ess render test\n");

    AvelEssProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    VocalSynth voc; voc.prepare (48000.0);

    check (proc.getLatencySamples() == 240 + 127, "reported latency = 5 ms lookahead + FIR centre (" + juce::String (proc.getLatencySamples()) + ")");

    std::unique_ptr<AvelEssEditor> ed (dynamic_cast<AvelEssEditor*> (proc.createEditor()));
    ed->setVisible (true);
    ed->setSize (1180, 720);

    runAudio (proc, voc, 380, ed.get());
    settle();
    snapshot (*ed, "main_1180x720");

    // reduction actually happens on this material
    {
        float peak = 0;
        AvelEssProcessor p2; p2.setPlayConfigDetails (2, 2, 48000.0, 512); p2.prepareToPlay (48000.0, 512);
        VocalSynth v2; v2.prepare (48000.0);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        for (int b = 0; b < 300; ++b)
        {
            for (int i = 0; i < 512; ++i) { const float x = v2.next(); buf.setSample (0, i, x); buf.setSample (1, i, x); }
            p2.processBlock (buf, midi);
            int s1, n1, s2, n2;
            p2.frameFifo.prepareToRead (p2.frameFifo.getNumReady(), s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) peak = juce::jmax (peak, p2.frameBuf[(size_t) (s1 + i)].grDb);
            for (int i = 0; i < n2; ++i) peak = juce::jmax (peak, p2.frameBuf[(size_t) (s2 + i)].grDb);
            p2.frameFifo.finishedRead (n1 + n2);
        }
        check (peak > 3.0f && peak <= 9.01f, "synthetic vocal: peak reduction within Range (" + juce::String (peak, 2) + " dB)");
    }

    // second look: Allround, wide band, narrow band, Side
    setParam (proc, "mode", 1); setParam (proc, "processing", 0); setParam (proc, "low", 3200); setParam (proc, "high", 9000);
    setParam (proc, "channels", 2); setParam (proc, "threshold", -38); setParam (proc, "audition", 1);
    runAudio (proc, voc, 260, ed.get());
    settle();
    snapshot (*ed, "allround_band_1180x720");

    // sizes
    proc.loadFactoryPreset (0);
    ed->setSize (1000, 620);  runAudio (proc, voc, 200, ed.get()); settle(); snapshot (*ed, "min_1000x620");
    ed->setSize (1500, 900); runAudio (proc, voc, 200, ed.get()); settle(); snapshot (*ed, "large_1500x900");
    ed->setSize (1180, 720);

    // presets
    proc.loadFactoryPreset (3);
    check (std::abs (proc.apvts.getRawParameterValue ("low")->load() - 4600.0f) < 1.0f, "factory preset 'Vocal Male' sets Band Low to 4.6 kHz");
    check (proc.apvts.getRawParameterValue ("range")->load() == 8.0f, "factory preset sets Range");
    proc.loadFactoryPreset (0);
    check (std::abs (proc.apvts.getRawParameterValue ("threshold")->load() + 26.0f) < 0.01f, "Default preset restores defaults");

    // state round trip
    {
        setParam (proc, "threshold", -33.3f); setParam (proc, "channels", 1); setParam (proc, "low", 7100);
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        AvelEssProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (other.apvts.getRawParameterValue ("threshold")->load() + 33.3f) < 0.05f
               && other.apvts.getRawParameterValue ("channels")->load() == 1.0f
               && std::abs (other.apvts.getRawParameterValue ("low")->load() - 7100.0f) < 2.0f, "state round trip");
        check (other.uiWidth == 1180 && other.uiHeight == 720, "window size saved in state");
    }

    // user preset file round trip
    {
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ess_test.avelpreset");
        setParam (proc, "release", 77.0f);
        check (proc.saveUserPreset (f), "user preset saved");
        setParam (proc, "release", 20.0f);
        check (proc.loadUserPreset (f) && std::abs (proc.apvts.getRawParameterValue ("release")->load() - 77.0f) < 0.2f, "user preset loaded");
        f.deleteFile();
    }

    // A/B
    {
        setParam (proc, "range", 5.0f);
        proc.selectSlot (1);
        setParam (proc, "range", 15.0f);
        proc.selectSlot (0);
        const float a = proc.apvts.getRawParameterValue ("range")->load();
        proc.selectSlot (1);
        const float b = proc.apvts.getRawParameterValue ("range")->load();
        check (std::abs (a - 5.0f) < 0.05f && std::abs (b - 15.0f) < 0.05f, "A/B keeps two independent settings");
        proc.selectSlot (0);
    }

    // undo
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        proc.undo.clearUndoHistory();
        proc.undo.beginNewTransaction();
        setParam (proc, "threshold", -12.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);   // APVTS flushes to its tree on a timer
        proc.undo.undo();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        check (std::abs (proc.apvts.getRawParameterValue ("threshold")->load() + 12.0f) > 0.5f, "undo reverts a parameter change");
    }

    ed.reset();
    std::printf (failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
