#pragma once

#include "ahe/AHETheme.h"
#include "ui/LevelSelector.h"

class DiceVoxProcessor;

// ---------------------------------------------------------------------------
// RollChamber — DiceVox's hero area.
//
//   [UFO]  DUSTY DEEP ECHO HALL        STYLE [Dreamy Pop    v]   [BACK] [ ROLL ]
//          Pitch · Lo-Fi · Delay · Reverb   [LOW | MED | HIGH]
//
// ROLL builds a new random vocal chain (DiceVoxProcessor::rollDice), steered
// by STYLE and intensity (LOW / MED / HIGH); BACK returns to the chain before
// the last roll. The
// name and the list of active effects follow the processor (also after host
// automation or a project load).
// ---------------------------------------------------------------------------
class RollChamber : public juce::Component,
                    private juce::Timer
{
public:
    explicit RollChamber (DiceVoxProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::TextButton& getRollButton() noexcept { return rollButton; }
    juce::TextButton& getBackButton() noexcept { return backButton; }

    static constexpr auto emptyTitle   = "READY TO ROLL";
    static constexpr auto emptySummary = "Pick a style and click ROLL for a new vocal effect chain";

private:
    void timerCallback() override;
    void refresh();
    float beamAmount() const; // 0..1 while the tractor beam animation runs

    static constexpr double beamMs = 900.0;
    double beamStart = -1.0;   // ms counter when the last ROLL beam started

    DiceVoxProcessor& processor;
    std::unique_ptr<juce::Drawable> ufo;
    juce::TextButton rollButton { "ROLL" }, backButton { "BACK" };
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment;
    LevelSelector levelSelector;

    juce::String title, summary;
    juce::Rectangle<int> iconArea, textArea, styleLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollChamber)
};
