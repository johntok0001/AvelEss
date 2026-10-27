#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "DSP/Engine.h"

class AvelEssProcessor : public juce::AudioProcessor
{
public:
    AvelEssProcessor();
    ~AvelEssProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Avel Ess"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    // presets
    juce::String presetName { "Default" };
    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    static juce::File userPresetFolder();

    // A/B
    int activeSlot = 0;
    void selectSlot (int slot);
    void copyActiveToOther();

    // UI state (window size) — saved with the session
    int uiWidth = 1180, uiHeight = 720;

    // metering, written by the audio thread, drained by the editor
    static constexpr int frameCapacity = 4096, sampleCapacity = 1 << 15;
    juce::AbstractFifo frameFifo { frameCapacity };
    std::vector<avel::dsp::Frame> frameBuf = std::vector<avel::dsp::Frame> ((size_t) frameCapacity);
    juce::AbstractFifo sampleFifo { sampleCapacity };
    std::vector<float> sampleBuf = std::vector<float> ((size_t) sampleCapacity);
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<bool> editorOpen { false };

private:
    avel::dsp::Engine engine;
    avel::dsp::Params readParams() const;
    juce::ValueTree slots[2];
    int currentProgram = 0;

    struct P
    {
        std::atomic<float>* mode, *threshold, *range, *release, *lookahead, *link, *processing,
                          *channels, *low, *high, *audition, *input, *output;
    } p {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelEssProcessor)
};
