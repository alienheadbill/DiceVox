#include "ChorusModule.h"

void ChorusModule::addParameters (ParamList& params) const
{
    const auto hzAttrs  = juce::AudioParameterFloatAttributes().withLabel ("Hz");
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    juce::NormalisableRange<float> rateRange (0.1f, 5.0f, 0.01f);
    rateRange.setSkewForCentre (1.0f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { rateId, version }, "Chorus Rate", rateRange, 0.8f, hzAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, version }, "Chorus Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pctAttrs));
}

void ChorusModule::attach (juce::AudioProcessorValueTreeState& state)
{
    rateHz   = state.getRawParameterValue (rateId);
    depthPct = state.getRawParameterValue (depthId);
    jassert (rateHz != nullptr && depthPct != nullptr);
}

void ChorusModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    // One mono chorus per channel.
    auto mono = spec;
    mono.numChannels = 1;

    for (auto* c : { &left, &right })
    {
        c->prepare (mono);
        c->setCentreDelay (10.0f); // ms: short, so it reads as a double, not an echo
        c->setFeedback (0.0f);
        c->setMix (0.5f);
    }

    applyParameters();
    reset();
}

void ChorusModule::reset()
{
    left.reset();
    right.reset();
}

void ChorusModule::applyParameters()
{
    const float rate  = juce::jlimit (0.1f, 5.0f, rateHz->load());
    const float depth = juce::jlimit (0.0f, 1.0f, depthPct->load() / 100.0f);

    left.setRate (rate);
    right.setRate (rate * rightRateRatio);
    left.setDepth (depth);
    right.setDepth (depth);
}

void ChorusModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    applyParameters();

    auto& block = context.getOutputBlock();

    auto l = block.getSingleChannelBlock (0);
    left.process (juce::dsp::ProcessContextReplacing<float> (l));

    if (block.getNumChannels() > 1)
    {
        auto r = block.getSingleChannelBlock (1);
        right.process (juce::dsp::ProcessContextReplacing<float> (r));
    }
}
