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
//   Style      each style has three recipes (e.g. Dark Toy Box: music box,
//              creepy phone, uncanny whisper): two core effects plus extras,
//              and how to set them. A roll never repeats the last recipe.
//              Any Vibe borrows recipes from every style, or rolls free-form.
//   Intensity  LOW = 1-2 effects leaning gentle, MED = 2-4, HIGH = 3-5
//              leaning wild. Settings range widely on every level.
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

    // Intensity levels, in Intensity-parameter order: Low, Medium, High.
    const juce::StringArray& levelNames();

    // How many recipes a style rolls from (Any Vibe: all of them).
    int numRecipesFor (int style);
}
