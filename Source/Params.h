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
}

namespace Params
{
    // Builds the full parameter layout: global controls + every module's params.
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const FxChain& chain);
}
