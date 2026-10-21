#include "FilterModule.h"
#include "FilterQTable.h"

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

float FilterModule::stageQ (float resonancePercent, float lowCutFreq, float highCutFreq, double rate)
{
    using namespace FilterQTable;

    // Spacing between the cutoffs in octaves, on the prewarped frequencies the
    // TPT filters really use, so the table's bound is exact at any sample rate.
    const auto prewarp = [rate] (float hz) { return std::tan (juce::MathConstants<double>::pi * hz / rate); };
    const auto octaves = (float) std::log2 (prewarp (highCutFreq) / prewarp (lowCutFreq));

    // Bilinear interpolation (continuous, so the limit never steps or clicks).
    const float res = juce::jlimit (0.0f, 100.0f, resonancePercent);
    const float fo  = juce::jlimit (0.0f, (float) (numOctaves - 1), (octaves - minOctaves) / octaveStep);
    const float fr  = juce::jlimit (0.0f, (float) (numResonances - 1), res / 100.0f / resonanceStep);
    const int o0 = juce::jmin ((int) fo, numOctaves - 2), r0 = juce::jmin ((int) fr, numResonances - 2);
    const float to = fo - (float) o0, tr = fr - (float) r0;

    const auto row = [&] (int r) { return qFraction[r][o0] + to * (qFraction[r][o0 + 1] - qFraction[r][o0]); };
    const float a = row (r0), b = row (r0 + 1);
    return resonanceToQ (res) * (a + tr * (b - a));
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
    resonanceSmoothed.reset (sampleRate, smoothingSeconds);

    reset();
}

void FilterModule::reset()
{
    // Start exactly at the current settings, with empty filter memory.
    setTargetsFromParameters();
    lowCutSmoothed.setCurrentAndTargetValue (lowCutSmoothed.getTargetValue());
    highCutSmoothed.setCurrentAndTargetValue (highCutSmoothed.getTargetValue());
    resonanceSmoothed.setCurrentAndTargetValue (resonanceSmoothed.getTargetValue());
    applyCoefficients (lowCutSmoothed.getCurrentValue(), highCutSmoothed.getCurrentValue(), resonanceSmoothed.getCurrentValue());

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
    resonanceSmoothed.setTargetValue (juce::jlimit (0.0f, 100.0f, resonancePct->load()));
}

void FilterModule::applyCoefficients (float lowCutFreq, float highCutFreq, float resonancePercent)
{
    const float q = stageQ (resonancePercent, lowCutFreq, highCutFreq, sampleRate);
    lowCut.setCutoffFrequency (lowCutFreq);
    highCut.setCutoffFrequency (highCutFreq);
    lowCut.setResonance (q);
    highCut.setResonance (q);
}

void FilterModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    setTargetsFromParameters();

    const bool smoothing = lowCutSmoothed.isSmoothing() || highCutSmoothed.isSmoothing() || resonanceSmoothed.isSmoothing();

    if (! smoothing)
    {
        // Settled: run whole blocks at the target settings (re-applied each block,
        // which costs a few tan() calls and never leaves stale coefficients).
        applyCoefficients (lowCutSmoothed.getCurrentValue(), highCutSmoothed.getCurrentValue(), resonanceSmoothed.getCurrentValue());
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
        applyCoefficients (lowCutSmoothed.getNextValue(), highCutSmoothed.getNextValue(), resonanceSmoothed.getNextValue());

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* samples = block.getChannelPointer (ch);
            samples[i] = highCut.processSample ((int) ch, lowCut.processSample ((int) ch, samples[i]));
        }
    }

    lowCut.snapToZero();
    highCut.snapToZero();
}
