#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class FxChain;

// Global (non-module) parameter IDs.
namespace ParamIDs
{
    inline constexpr auto inputGain  = "input_gain";
    inline constexpr auto outputGain = "output_gain";
    inline constexpr auto mix        = "mix";
    inline constexpr auto bypass     = "bypass";

    // Dice controls (they steer ROLL; they never change the sound by themselves).
    inline constexpr auto diceLevel     = "dice_level";   // Low / Medium / High
    inline constexpr auto diceStyle     = "dice_style";
    inline juce::String diceLock (const juce::String& moduleId) { return "dice_lock_" + moduleId; }

    inline bool isDiceControl (const juce::String& id) { return id.startsWith ("dice_"); }
}

namespace Params
{
    // Builds the full parameter layout: global controls + every module's params
    // + the dice controls (intensity, style, one lock per module).
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const FxChain& chain);
}
