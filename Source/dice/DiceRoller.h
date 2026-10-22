#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// DiceRoller — builds a random vocal effect chain.
//
// A roll picks 2-4 of the effects (reverb and delay come up most often),
// gives each one settings from a range that sounds good on vocals (narrower
// than the full knob range), switches the others off, and returns a name
// for the result, e.g. "Gritty Wide Echo Hall".
//
// It only sets parameters, through the host-facing API with change
// gestures, so a roll behaves exactly like the user turning the knobs: host
// undo, automation recording and project recall all work. Input, Mix,
// Output and Bypass are never touched. Call it on the message thread.
// ---------------------------------------------------------------------------
namespace DiceRoller
{
    juce::String roll (juce::AudioProcessorValueTreeState& state, juce::Random& rng);

    // IDs of the effects the dice can switch on, in chain order.
    const juce::StringArray& moduleIds();
}
