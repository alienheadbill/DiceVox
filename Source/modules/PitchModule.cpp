#include "PitchModule.h"

void PitchModule::addParameters (ParamList& params) const
{
    const auto stAttrs = juce::AudioParameterFloatAttributes()
                             .withLabel ("st")
                             .withStringFromValueFunction ([] (float v, int)
                             {
                                 const auto rounded = juce::roundToInt (v * 10.0f);
                                 return (rounded > 0 ? "+" : "") + juce::String ((float) rounded / 10.0f, 1);
                             });
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shiftId, version }, "Pitch Shift",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), -12.0f, stAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, version }, "Pitch Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pctAttrs));
}

void PitchModule::attach (juce::AudioProcessorValueTreeState& state)
{
    semitones = state.getRawParameterValue (shiftId);
    mixPct    = state.getRawParameterValue (mixId);
    jassert (semitones != nullptr && mixPct != nullptr);
}

void PitchModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    shifter.prepare (spec.sampleRate, (int) spec.numChannels, windowSeconds);
    shifter.setWindowSeconds (windowSeconds);
    dry.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, true, false);
    mix.reset (spec.sampleRate, 0.05);
    reset();
}

void PitchModule::reset()
{
    shifter.reset();
    mix.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));
}

void PitchModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) dry.getNumChannels());
    const auto numSamples  = juce::jmin (block.getNumSamples(), (size_t) dry.getNumSamples());

    shifter.setRatio (std::pow (2.0f, juce::jlimit (-12.0f, 12.0f, semitones->load()) / 12.0f));
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));

    auto dryBlock = juce::dsp::AudioBlock<float> (dry).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
    auto wetBlock = block.getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
    dryBlock.copyFrom (wetBlock);
    shifter.process (wetBlock);

    // Constant-power blend: the level stays even across the Mix range.
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float m = mix.getNextValue();
        const float dryGain = std::cos (m * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin (m * juce::MathConstants<float>::halfPi);

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto& w = wetBlock.getChannelPointer (ch)[i];
            w = dryGain * dryBlock.getChannelPointer (ch)[i] + wetGain * w;
        }
    }
}
