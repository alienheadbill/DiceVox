#include "ReverbModule.h"

juce::Reverb::Parameters ReverbModule::makeParameters (float sizePercent, float mixPercent)
{
    const float size = juce::jlimit (0.0f, 1.0f, sizePercent / 100.0f);
    const float mix  = juce::jlimit (0.0f, 1.0f, mixPercent / 100.0f);

    // juce::Reverb scales dryLevel by 2 and wetLevel by 3 internally, so
    // dryLevel 0.5 is the untouched vocal.
    juce::Reverb::Parameters p;
    p.roomSize   = 0.30f + 0.68f * size;
    p.damping    = 0.45f;
    p.width      = 1.0f;
    p.wetLevel   = 0.40f * mix;
    p.dryLevel   = 0.5f * (1.0f - 0.4f * mix * mix);
    p.freezeMode = 0.0f;
    return p;
}

void ReverbModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sizeId, version }, "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, version }, "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pctAttrs));
}

void ReverbModule::attach (juce::AudioProcessorValueTreeState& state)
{
    sizePct = state.getRawParameterValue (sizeId);
    mixPct  = state.getRawParameterValue (mixId);
    jassert (sizePct != nullptr && mixPct != nullptr);
}

void ReverbModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    reverb.prepare (spec);
    lastSize = lastMix = -1.0f;
    applyParameters();
    reset();
}

void ReverbModule::reset()
{
    reverb.reset();
}

void ReverbModule::applyParameters()
{
    const float size = sizePct->load(), mix = mixPct->load();
    if (juce::exactlyEqual (size, lastSize) && juce::exactlyEqual (mix, lastMix))
        return;

    lastSize = size;
    lastMix  = mix;
    reverb.setParameters (makeParameters (size, mix));
}

void ReverbModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    applyParameters();
    reverb.process (context);
}
