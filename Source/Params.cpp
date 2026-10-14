#include "Params.h"
#include "modules/FxChain.h"

namespace Params
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const FxChain& chain)
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        auto dbAttrs = juce::AudioParameterFloatAttributes().withLabel ("dB");
        auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::inputGain, 1 }, "Input",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dbAttrs));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::outputGain, 1 }, "Output",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dbAttrs));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttrs));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));

        chain.addParameters (params);

        return { params.begin(), params.end() };
    }
}
