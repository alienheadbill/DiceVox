#include "FilterModule.h"

namespace
{
    constexpr double smoothingSeconds = 0.05;

    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (std::sqrt (lo * hi)); // knob centre = geometric middle
        return r;
    }
}

float FilterModule::resonanceToQ (float percent)
{
    const float r = juce::jlimit (0.0f, 1.0f, percent / 100.0f);
    return minQ * std::pow (maxQ / minQ, r);
}

void FilterModule::addParameters (ParamList& params) const
{
    const auto hzAttrs = juce::AudioParameterFloatAttributes()
                             .withLabel ("Hz")
                             .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); });
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowCutId, version }, "Filter Low Cut",
        logRange (20.0f, 2000.0f), 80.0f, hzAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highCutId, version }, "Filter High Cut",
        logRange (200.0f, 20000.0f), 20000.0f, hzAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { resonanceId, version }, "Filter Resonance",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctAttrs));
}

void FilterModule::attach (juce::AudioProcessorValueTreeState& state)
{
    lowCutHz     = state.getRawParameterValue (lowCutId);
    highCutHz    = state.getRawParameterValue (highCutId);
    resonancePct = state.getRawParameterValue (resonanceId);
    jassert (lowCutHz != nullptr && highCutHz != nullptr && resonancePct != nullptr);
}

void FilterModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowCut.prepare (spec);
    highCut.prepare (spec);

    for (auto* s : { &lowCutSmoothed, &highCutSmoothed })
        s->reset (sampleRate, smoothingSeconds);
    qSmoothed.reset (sampleRate, smoothingSeconds);

    reset();
}

void FilterModule::reset()
{
    // Start exactly at the current settings, with empty filter memory.
    setTargetsFromParameters();
    lowCutSmoothed.setCurrentAndTargetValue (lowCutSmoothed.getTargetValue());
    highCutSmoothed.setCurrentAndTargetValue (highCutSmoothed.getTargetValue());
    qSmoothed.setCurrentAndTargetValue (qSmoothed.getTargetValue());
    applyCoefficients (lowCutSmoothed.getCurrentValue(), highCutSmoothed.getCurrentValue(), qSmoothed.getCurrentValue());

    lowCut.reset();
    highCut.reset();
}

float FilterModule::clampCutoff (float hz) const noexcept
{
    // The SVF needs cutoff < Nyquist; stay a little under it at low sample rates.
    return juce::jlimit (10.0f, (float) (sampleRate * 0.45), hz);
}

void FilterModule::setTargetsFromParameters()
{
    lowCutSmoothed.setTargetValue  (clampCutoff (lowCutHz->load()));
    highCutSmoothed.setTargetValue (clampCutoff (highCutHz->load()));
    qSmoothed.setTargetValue (resonanceToQ (resonancePct->load()));
}

void FilterModule::applyCoefficients (float lowCutFreq, float highCutFreq, float q)
{
    lowCut.setCutoffFrequency (lowCutFreq);
    highCut.setCutoffFrequency (highCutFreq);
    lowCut.setResonance (q);
    highCut.setResonance (q);
}

void FilterModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    setTargetsFromParameters();

    const bool smoothing = lowCutSmoothed.isSmoothing() || highCutSmoothed.isSmoothing() || qSmoothed.isSmoothing();

    if (! smoothing)
    {
        // Settled: run whole blocks at the target settings (re-applied each block,
        // which costs two tan() calls and never leaves stale coefficients).
        applyCoefficients (lowCutSmoothed.getCurrentValue(), highCutSmoothed.getCurrentValue(), qSmoothed.getCurrentValue());
        lowCut.process (context);
        highCut.process (context);
        return;
    }

    // Moving: update coefficients every sample so cutoff sweeps stay smooth.
    auto& block = context.getOutputBlock();
    const auto numChannels = block.getNumChannels();
    const auto numSamples  = block.getNumSamples();

    for (size_t i = 0; i < numSamples; ++i)
    {
        applyCoefficients (lowCutSmoothed.getNextValue(), highCutSmoothed.getNextValue(), qSmoothed.getNextValue());

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* samples = block.getChannelPointer (ch);
            samples[i] = highCut.processSample ((int) ch, lowCut.processSample ((int) ch, samples[i]));
        }
    }

    lowCut.snapToZero();
    highCut.snapToZero();
}
