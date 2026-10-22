#include "DriveModule.h"

namespace
{
    constexpr double smoothingSeconds = 0.05;

    // Output scaling: a signal peaking at this level comes out at the same
    // level whatever the drive, so a normal vocal keeps its loudness while
    // louder peaks are squashed and quiet parts come up (like a compressor).
    constexpr float referenceLevel = 0.25f;

    float makeupFor (float gain)
    {
        return referenceLevel / std::tanh (gain * referenceLevel);
    }
}

float DriveModule::amountToGain (float percent)
{
    return juce::Decibels::decibelsToGain (24.0f * juce::jlimit (0.0f, 100.0f, percent) / 100.0f);
}

float DriveModule::toneToCutoff (float percent)
{
    return 2000.0f * std::pow (10.0f, juce::jlimit (0.0f, 100.0f, percent) / 100.0f);
}

void DriveModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { amountId, version }, "Drive Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pctAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { toneId, version }, "Drive Tone",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, pctAttrs));
}

void DriveModule::attach (juce::AudioProcessorValueTreeState& state)
{
    amountPct = state.getRawParameterValue (amountId);
    tonePct   = state.getRawParameterValue (toneId);
    jassert (amountPct != nullptr && tonePct != nullptr);
}

void DriveModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    gain.reset (sampleRate, smoothingSeconds);
    cutoff.reset (sampleRate, smoothingSeconds);
    reset();
}

void DriveModule::reset()
{
    setTargetsFromParameters();
    gain.setCurrentAndTargetValue (gain.getTargetValue());
    cutoff.setCurrentAndTargetValue (cutoff.getTargetValue());
    toneState.fill (0.0f);
}

void DriveModule::setTargetsFromParameters()
{
    gain.setTargetValue (amountToGain (amountPct->load()));
    cutoff.setTargetValue (juce::jmin (toneToCutoff (tonePct->load()), (float) (sampleRate * 0.45)));
}

void DriveModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    setTargetsFromParameters();

    auto& block = context.getOutputBlock();
    const auto numChannels = juce::jmin (block.getNumChannels(), toneState.size());
    const auto numSamples  = block.getNumSamples();
    const auto twoPiOverRate = juce::MathConstants<float>::twoPi / (float) sampleRate;

    const bool smoothing = gain.isSmoothing() || cutoff.isSmoothing();
    float g = gain.getCurrentValue();
    float makeup = makeupFor (g);
    float coeff = 1.0f - std::exp (-twoPiOverRate * cutoff.getCurrentValue());

    for (size_t i = 0; i < numSamples; ++i)
    {
        if (smoothing)
        {
            g = gain.getNextValue();
            makeup = makeupFor (g);
            coeff = 1.0f - std::exp (-twoPiOverRate * cutoff.getNextValue());
        }

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto& x = block.getChannelPointer (ch)[i];
            const float saturated = std::tanh (g * x) * makeup;
            auto& lp = toneState[ch];
            lp += coeff * (saturated - lp);
            x = lp;
        }
    }

    for (auto& s : toneState)
        if (std::abs (s) < 1.0e-15f)
            s = 0.0f;
}
