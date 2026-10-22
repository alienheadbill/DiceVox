#include "ReverseModule.h"

const juce::StringArray& ReverseModule::lengthNames()
{
    static const juce::StringArray names { "1/16", "1/8", "1/4", "1/2" };
    return names;
}

double ReverseModule::lengthBeats (int index)
{
    static constexpr double beats[] = { 0.25, 0.5, 1.0, 2.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

double ReverseModule::sliceSeconds (int lengthIndex, double bpm)
{
    const double safeBpm = bpm > 1.0 ? bpm : 120.0;
    return juce::jmin (maxSliceSeconds, lengthBeats (lengthIndex) * 60.0 / safeBpm);
}

void ReverseModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { lengthId, version }, "Reverse Length", lengthNames(), defaultLength));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, version }, "Reverse Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pctAttrs));
}

void ReverseModule::attach (juce::AudioProcessorValueTreeState& state)
{
    lengthChoice = state.getRawParameterValue (lengthId);
    mixPct       = state.getRawParameterValue (mixId);
    jassert (lengthChoice != nullptr && mixPct != nullptr);
}

void ReverseModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    // A reverse grain plays half a window backwards, so the window is two slices.
    shifter.prepare (spec.sampleRate, (int) spec.numChannels, 2.0 * maxSliceSeconds);
    shifter.setRatio (-1.0f);
    wet.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, true, false);
    mix.reset (spec.sampleRate, 0.05);
    reset();
}

void ReverseModule::reset()
{
    updateWindow();
    shifter.reset(); // also snaps the window to the current length
    mix.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));
}

void ReverseModule::setHostTempo (double bpm)
{
    if (bpm > 1.0)
        tempo = bpm;
}

void ReverseModule::updateWindow()
{
    shifter.setWindowSeconds (2.0 * sliceSeconds (juce::roundToInt (lengthChoice->load()), tempo));
}

void ReverseModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    updateWindow();
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));

    auto& block = context.getOutputBlock();
    const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) wet.getNumChannels());
    const auto numSamples  = juce::jmin (block.getNumSamples(), (size_t) wet.getNumSamples());

    auto wetBlock = juce::dsp::AudioBlock<float> (wet).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
    wetBlock.copyFrom (block.getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples));
    shifter.process (wetBlock);

    // Add the reversed slices on top of the untouched vocal.
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float m = mix.getNextValue();
        for (size_t ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (ch)[i] += m * wetBlock.getChannelPointer (ch)[i];
    }
}
