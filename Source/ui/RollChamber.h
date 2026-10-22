#pragma once

#include "ahe/AHETheme.h"

class DiceVoxProcessor;

// ---------------------------------------------------------------------------
// RollChamber — DiceVox's hero area.
//
//   [UFO]  GRITTY WIDE ECHO HALL                 [BACK] [  ROLL  ]
//          Filter + Drive + Delay + Reverb
//
// ROLL builds a new random vocal chain (DiceVoxProcessor::rollDice), BACK
// returns to the chain before the last roll. The name and the list of active
// effects follow the processor (also after host automation or a project load).
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
    static constexpr auto emptySummary = "Click ROLL for a new vocal effect chain";

private:
    void timerCallback() override;
    void refresh();

    DiceVoxProcessor& processor;
    std::unique_ptr<juce::Drawable> ufo;
    juce::TextButton rollButton { "ROLL" }, backButton { "BACK" };

    juce::String title, summary;
    juce::Rectangle<int> iconArea, textArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollChamber)
};
