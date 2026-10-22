#include "ChopModule.h"

const juce::StringArray& ChopModule::rateNames()
{
    static const juce::StringArray names { "1/32", "1/16", "1/16T", "1/8", "1/8T", "1/4" };
    return names;
}

double ChopModule::rateBeats (int index)
{
    static constexpr double beats[] = { 0.125, 0.25, 1.0 / 6.0, 0.5, 1.0 / 3.0, 1.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

void ChopModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { rateId, version }, "Chop Rate", rateNames(), defaultRate));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, version }, "Chop Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f, pctAttrs));
}

void ChopModule::attach (juce::AudioProcessorValueTreeState& state)
{
    rateChoice = state.getRawParameterValue (rateId);
    depthPct   = state.getRawParameterValue (depthId);
    jassert (rateChoice != nullptr && depthPct != nullptr);
}

void ChopModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    edgeCoeff = 1.0f - std::exp (-1.0f / (float) (0.002 * sampleRate)); // ~2 ms edges
    reset();
}

void ChopModule::reset()
{
    beats = 0.0;
    pendingPpq = -1.0;
    gain = 1.0f;
}

void ChopModule::setHostTempo (double bpm)
{
    if (bpm > 1.0)
        tempo = bpm;
}

void ChopModule::setHostPosition (double ppq)
{
    pendingPpq = ppq;
}

void ChopModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    if (pendingPpq >= 0.0)
    {
        beats = pendingPpq; // lock to the song's beat grid
        pendingPpq = -1.0;
    }

    const double step = rateBeats (juce::roundToInt (rateChoice->load()));
    const double beatsPerSample = tempo / 60.0 / sampleRate;
    const float low = 1.0f - juce::jlimit (0.0f, 1.0f, depthPct->load() / 100.0f);

    auto& block = context.getOutputBlock();
    const auto numChannels = block.getNumChannels();
    const auto numSamples  = block.getNumSamples();

    for (size_t i = 0; i < numSamples; ++i)
    {
        // Open for the first half of each step, closed (down to `low`) for the second.
        const double positionInStep = beats / step - std::floor (beats / step);
        const float target = positionInStep < 0.5 ? 1.0f : low;
        gain += edgeCoeff * (target - gain);
        beats += beatsPerSample;

        for (size_t ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (ch)[i] *= gain;
    }
}
