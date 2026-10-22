#include "DelayModule.h"

namespace
{
    constexpr double timeGlideSeconds  = 0.25;
    constexpr double levelSmoothSeconds = 0.05;
    constexpr float  repeatLowCutHz  = 150.0f;
    constexpr float  repeatHighCutHz = 5000.0f;

    float onePoleCoeff (float hz, double rate)
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) rate);
    }
}

const juce::StringArray& DelayModule::divisionNames()
{
    static const juce::StringArray names { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
    return names;
}

double DelayModule::divisionBeats (int index)
{
    static constexpr double beats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

double DelayModule::delaySeconds (int divisionIndex, double bpm)
{
    const double safeBpm = bpm > 1.0 ? bpm : 120.0;
    return juce::jmin (maxDelaySeconds, divisionBeats (divisionIndex) * 60.0 / safeBpm);
}

void DelayModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { timeId, version }, "Delay Time", divisionNames(), defaultDivision));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackId, version }, "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, maxFeedback, 0.1f), 35.0f, pctAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, version }, "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pctAttrs));
}

void DelayModule::attach (juce::AudioProcessorValueTreeState& state)
{
    division    = state.getRawParameterValue (timeId);
    feedbackPct = state.getRawParameterValue (feedbackId);
    mixPct      = state.getRawParameterValue (mixId);
    jassert (division != nullptr && feedbackPct != nullptr && mixPct != nullptr);
}

void DelayModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    line.setMaximumDelayInSamples ((int) std::ceil (maxDelaySeconds * sampleRate) + 4);
    line.prepare (spec);

    lowCoeff  = onePoleCoeff (repeatLowCutHz, sampleRate);
    highCoeff = onePoleCoeff (juce::jmin (repeatHighCutHz, (float) (sampleRate * 0.45)), sampleRate);

    delaySamples.reset (sampleRate, timeGlideSeconds);
    feedback.reset (sampleRate, levelSmoothSeconds);
    wet.reset (sampleRate, levelSmoothSeconds);

    reset();
}

void DelayModule::reset()
{
    setTargetsFromParameters();
    delaySamples.setCurrentAndTargetValue (delaySamples.getTargetValue());
    feedback.setCurrentAndTargetValue (feedback.getTargetValue());
    wet.setCurrentAndTargetValue (wet.getTargetValue());

    line.reset();
    lowState.fill (0.0f);
    highState.fill (0.0f);
}

void DelayModule::setHostTempo (double bpm)
{
    if (bpm > 1.0)
        tempo = bpm;
}

void DelayModule::setTargetsFromParameters()
{
    const auto seconds = delaySeconds (juce::roundToInt (division->load()), tempo);
    delaySamples.setTargetValue ((float) juce::jmax (1.0, seconds * sampleRate));
    feedback.setTargetValue (juce::jlimit (0.0f, maxFeedback, feedbackPct->load()) / 100.0f);
    wet.setTargetValue (juce::jlimit (0.0f, 100.0f, mixPct->load()) / 100.0f);
}

float DelayModule::filterRepeat (size_t channel, float x) noexcept
{
    // Band-limit the repeat: high cut (one-pole low-pass), then low cut
    // (input minus a one-pole low-pass at 150 Hz).
    auto& hi = highState[channel];
    hi += highCoeff * (x - hi);
    auto& lo = lowState[channel];
    lo += lowCoeff * (hi - lo);
    return hi - lo;
}

void DelayModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    setTargetsFromParameters();

    auto& block = context.getOutputBlock();
    const auto numSamples = block.getNumSamples();
    const bool stereo = block.getNumChannels() > 1;

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float d  = delaySamples.getNextValue();
        const float fb = feedback.getNextValue();
        const float w  = wet.getNextValue();

        if (stereo)
        {
            auto& l = block.getChannelPointer (0)[i];
            auto& r = block.getChannelPointer (1)[i];

            const float tapL = line.popSample (0, d);
            const float tapR = line.popSample (1, d);

            // Ping-pong: the vocal enters mostly the left line (a little on the
            // right so a single echo isn't hard-panned), each line feeds the other.
            const float in = 0.5f * (l + r);
            line.pushSample (0, std::tanh (in + fb * filterRepeat (0, tapR)));
            line.pushSample (1, std::tanh (0.35f * in + fb * filterRepeat (1, tapL)));

            l += w * tapL;
            r += w * tapR;
        }
        else
        {
            auto& x = block.getChannelPointer (0)[i];
            const float tap = line.popSample (0, d);
            line.pushSample (0, std::tanh (x + fb * filterRepeat (0, tap)));
            x += w * tap;
        }
    }

    for (auto* states : { &lowState, &highState })
        for (auto& s : *states)
            if (std::abs (s) < 1.0e-15f)
                s = 0.0f;
}
