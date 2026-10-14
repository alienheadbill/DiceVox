#pragma once

#include "ahe/AHETheme.h"

// ---------------------------------------------------------------------------
// RollChamber — DiceVox's hero area for the ROLL (randomize) action.
//
// Phase 1: the randomizer does not exist yet, so this is deliberately shown as
// UNAVAILABLE: muted colours, a "coming in Phase 3" line and a tooltip. It is
// not clickable, has no animation and never implies effects were generated.
// Phase 3 replaces the body with the real ROLL button.
// ---------------------------------------------------------------------------
class RollChamber : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    RollChamber();

    void paint (juce::Graphics&) override;

    static constexpr auto unavailableText = "Randomizer coming in Phase 3";

private:
    std::unique_ptr<juce::Drawable> ufo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollChamber)
};
