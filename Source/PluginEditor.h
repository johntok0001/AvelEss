#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Views.h"

class AvelEssEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AvelEssEditor (AvelEssProcessor&);
    ~AvelEssEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // used by the offline render test
    void pumpForTest() { timerCallback(); }

private:
    void timerCallback() override;
    void showPresetMenu();
    void stepPreset (int delta);
    void refreshDependents();

    AvelEssProcessor& proc;
    avel::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // top bar
    avel::ui::IconButton presetButton, prevPreset { avel::ui::icon::chevronLeft() }, nextPreset { avel::ui::icon::chevronRight() },
                         undoButton { avel::ui::icon::undo() }, redoButton { avel::ui::icon::redo() },
                         slotA, slotB, copySlot { avel::ui::icon::copy() };
    avel::ui::Segmented mode { { "Single Vocal", "Allround" } };

    // display
    avel::ui::HistoryView history;
    avel::ui::BandView band;

    // editor card
    avel::ui::Dial threshold { "Threshold" }, range { "Range" }, release { "Release" }, lookahead { "Lookahead" }, link { "Link" },
                   low { "Band Low" }, high { "Band High" }, input { "Input" }, output { "Output" };
    avel::ui::Segmented processing { { "Wide Band", "Split Band" } }, channels { { "Stereo", "Mid", "Side" } };
    avel::ui::Switch audition;

    // right column
    avel::ui::MeterColumn meters;

    juce::Rectangle<float> displayArea, cardArea, rightArea;
    juce::Rectangle<int> processingCaption, channelsCaption, auditionCaption;
    float lastIn = -120, lastGr = 0, lastOut = -120;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelEssEditor)
};
