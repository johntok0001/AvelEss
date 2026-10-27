#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 [] (float, float, float v) { return v; } };
    }

    juce::String freqText (float v, int)
    {
        if (v >= 19950.0f) return "Open";
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    }
    float freqValue (const juce::String& s)
    {
        auto t = s.trim().toLowerCase();
        if (t.startsWith ("open") || t.startsWith ("off")) return 20000.0f;
        float v = t.getFloatValue();
        if (t.containsChar ('k')) v *= 1000.0f;
        else if (v < 30.0f) v *= 1000.0f;     // "5.5" means 5.5 kHz
        return v;
    }
    juce::String dbText (float v, int)    { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String grText (float v, int)    { return juce::String (v, 1) + " dB"; }
    juce::String msText (float v, int)    { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String pctText (float v, int)   { return juce::String (juce::roundToInt (v)) + " %"; }
    float numValue (const juce::String& s) { return s.retainCharacters ("0123456789.-+").getFloatValue(); }

    juce::AudioParameterFloatAttributes attr (juce::String (*fn) (float, int), float (*parse) (const juce::String&) = numValue)
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction (fn).withValueFromStringFunction (parse);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AvelEssProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };

    v.push_back (std::make_unique<AudioParameterChoice> (id ("mode"), "Mode", StringArray { "Single Vocal", "Allround" }, 0));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("threshold"), "Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -26.0f, attr (dbText)));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("range"), "Range", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 9.0f, attr (grText)));
    { NormalisableRange<float> r (5.0f, 250.0f, 0.1f); r.setSkewForCentre (40.0f);
      v.push_back (std::make_unique<AudioParameterFloat> (id ("release"), "Release", r, 40.0f, attr (msText))); }
    v.push_back (std::make_unique<AudioParameterFloat> (id ("lookahead"), "Lookahead", NormalisableRange<float> (0.0f, 15.0f, 0.1f), 5.0f, attr (msText)));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("link"), "Stereo Link", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, attr (pctText)));
    v.push_back (std::make_unique<AudioParameterChoice> (id ("processing"), "Processing", StringArray { "Wide Band", "Split Band" }, 1));
    v.push_back (std::make_unique<AudioParameterChoice> (id ("channels"), "Channels", StringArray { "Stereo", "Mid", "Side" }, 0));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("low"), "Band Low", logRange (1000.0f, 16000.0f), 5500.0f, attr (freqText, freqValue)));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("high"), "Band High", logRange (2000.0f, 20000.0f), 20000.0f, attr (freqText, freqValue)));
    v.push_back (std::make_unique<AudioParameterChoice> (id ("audition"), "Audition", StringArray { "Off", "On" }, 0));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("input"), "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, attr (dbText)));
    v.push_back (std::make_unique<AudioParameterFloat> (id ("output"), "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, attr (dbText)));
    return { v.begin(), v.end() };
}

AvelEssProcessor::AvelEssProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "AvelEss", createLayout())
{
    auto get = [this] (const char* s) { return apvts.getRawParameterValue (s); };
    p = { get ("mode"), get ("threshold"), get ("range"), get ("release"), get ("lookahead"), get ("link"), get ("processing"),
          get ("channels"), get ("low"), get ("high"), get ("audition"), get ("input"), get ("output") };
    slots[0] = apvts.copyState();
    slots[1] = apvts.copyState();
}

bool AvelEssProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void AvelEssProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencyFor (p.lookahead->load()));
}

avel::dsp::Params AvelEssProcessor::readParams() const
{
    avel::dsp::Params q;
    q.mode        = (int) p.mode->load();
    q.thresholdDb = p.threshold->load();
    q.rangeDb     = p.range->load();
    q.releaseMs   = p.release->load();
    q.lookaheadMs = p.lookahead->load();
    q.link        = p.link->load() * 0.01f;
    q.processing  = (int) p.processing->load();
    q.channels    = (int) p.channels->load();
    q.lowHz       = p.low->load();
    q.highHz      = p.high->load();
    q.audition    = p.audition->load() > 0.5f;
    q.inputDb     = p.input->load();
    q.outputDb    = p.output->load();
    return q;
}

void AvelEssProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), numCh = juce::jmin (2, getTotalNumInputChannels());
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    const auto q = readParams();
    const int lat = engine.latencyFor (q.lookaheadMs);
    if (lat != getLatencySamples()) setLatencySamples (lat);

    // spectrum feed (input, mono, pre-gain is fine for a visual)
    if (editorOpen.load (std::memory_order_relaxed))
    {
        const auto* l = buffer.getReadPointer (0);
        const auto* r = numCh > 1 ? buffer.getReadPointer (1) : l;
        int s1, n1, s2, n2;
        sampleFifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) sampleBuf[(size_t) (s1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < n2; ++i) sampleBuf[(size_t) (s2 + i)] = 0.5f * (l[n1 + i] + r[n1 + i]);
        sampleFifo.finishedWrite (n1 + n2);
    }

    float* ptr[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (ptr, numCh, n, q);

    const auto& f = engine.getFrames();
    if (! f.empty())
    {
        int s1, n1, s2, n2;
        frameFifo.prepareToWrite ((int) f.size(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) frameBuf[(size_t) (s1 + i)] = f[(size_t) i];
        for (int i = 0; i < n2; ++i) frameBuf[(size_t) (s2 + i)] = f[(size_t) (n1 + i)];
        frameFifo.finishedWrite (n1 + n2);
    }
}

//==============================================================================
int AvelEssProcessor::getNumPrograms() { return (int) std::size (avel::ess::factoryPresets); }
void AvelEssProcessor::setCurrentProgram (int i) { if (juce::isPositiveAndBelow (i, getNumPrograms())) loadFactoryPreset (i); }
const juce::String AvelEssProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String::fromUTF8 (avel::ess::factoryPresets[i].name) : juce::String();
}

void AvelEssProcessor::loadFactoryPreset (int index)
{
    const auto& fp = avel::ess::factoryPresets[index];
    undo.beginNewTransaction();
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            float v = r->getNormalisableRange().convertFrom0to1 (r->getDefaultValue());
            for (auto& kv : fp.values)
                if (r->getParameterID() == kv.first) v = kv.second;
            r->beginChangeGesture();
            r->setValueNotifyingHost (r->convertTo0to1 (v));
            r->endChangeGesture();
        }
    currentProgram = index;
    presetName = juce::String::fromUTF8 (fp.name);
}

juce::File AvelEssProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Avel/Ess/Presets");
}

bool AvelEssProcessor::saveUserPreset (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    if (auto xml = state.createXml())
    {
        file.getParentDirectory().createDirectory();
        if (xml->writeTo (file)) { presetName = file.getFileNameWithoutExtension(); return true; }
    }
    return false;
}

bool AvelEssProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            undo.beginNewTransaction();
            auto tree = juce::ValueTree::fromXml (*xml);
            for (auto* prm : getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    auto child = tree.getChildWithProperty ("id", r->getParameterID());
                    if (child.isValid())
                    {
                        r->beginChangeGesture();
                        r->setValueNotifyingHost (r->convertTo0to1 ((float) child.getProperty ("value")));
                        r->endChangeGesture();
                    }
                }
            presetName = file.getFileNameWithoutExtension();
            return true;
        }
    return false;
}

//==============================================================================
void AvelEssProcessor::selectSlot (int slot)
{
    if (slot == activeSlot) return;
    slots[activeSlot] = apvts.copyState();
    slots[activeSlot].setProperty ("presetName", presetName, nullptr);
    activeSlot = slot;
    undo.beginNewTransaction();
    auto t = slots[slot].createCopy();
    presetName = t.getProperty ("presetName", presetName).toString();
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            auto child = t.getChildWithProperty ("id", r->getParameterID());
            if (child.isValid())
            {
                r->beginChangeGesture();
                r->setValueNotifyingHost (r->convertTo0to1 ((float) child.getProperty ("value")));
                r->endChangeGesture();
            }
        }
}

void AvelEssProcessor::copyActiveToOther()
{
    slots[1 - activeSlot] = apvts.copyState();
    slots[1 - activeSlot].setProperty ("presetName", presetName, nullptr);
}

//==============================================================================
void AvelEssProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("uiWidth", uiWidth, nullptr);
    state.setProperty ("uiHeight", uiHeight, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AvelEssProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            presetName = tree.getProperty ("presetName", "Default").toString();
            uiWidth  = (int) tree.getProperty ("uiWidth", 1180);
            uiHeight = (int) tree.getProperty ("uiHeight", 720);
            apvts.replaceState (tree);
            undo.clearUndoHistory();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AvelEssProcessor(); }
