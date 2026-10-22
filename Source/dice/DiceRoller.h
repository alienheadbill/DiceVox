#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// DiceRoller — builds a random vocal effect chain.
//
// A roll switches on a handful of effects, gives each one settings from a
// range that sounds good on vocals, switches the others off, and returns a
// name for the result, e.g. "Dusty Deep Echo Hall". It is steered by the
// dice controls (Params.h):
//
//   Intensity  0 % = 1-3 subtle effects ... 100 % = 3-5 effects pushed hard.
//   Style      which effects come up and how they are set (see styleNames()).
//   Locks      a locked effect keeps its on/off state and settings; the dice
//              only reroll the unlocked ones.
//
// It only sets parameters, through the host-facing API with change
// gestures, so a roll behaves exactly like the user turning the knobs: host
// undo, automation recording and project recall all work. Input, Mix,
// Output, Bypass and the dice controls are never touched. Call it on the
// message thread.
// ---------------------------------------------------------------------------
namespace DiceRoller
{
    juce::String roll (juce::AudioProcessorValueTreeState& state, juce::Random& rng);

    // IDs of the effects the dice can switch on, in chain order.
    const juce::StringArray& moduleIds();

    // Dice styles, in Style-parameter order. Named after the sound, not artists.
    const juce::StringArray& styleNames();
}
