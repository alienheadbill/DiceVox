#include "AlienModule.h"

namespace
{
    constexpr float combFeedback = 0.82f;
    constexpr float minRobotHz   = 60.0f;

    float onePoleCoeff (float hz, double rate)
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) rate);
    }
}

const juce::StringArray& AlienModule::modeNames()
{
    static const juce::StringArray names { "Saucer", "Android", "Transmission" };
    return names;
}

float AlienModule::ringFrequency (float amountPercent)
{
    return 20.0f * std::pow (60.0f, juce::jlimit (0.0f, 100.0f, amountPercent) / 100.0f); // 20 Hz .. 1.2 kHz
}

float AlienModule::robotFrequency (float amountPercent)
{
    return minRobotHz * std::pow (4.0f, juce::jlimit (0.0f, 100.0f, amountPercent) / 100.0f); // 60 .. 240 Hz
}

void AlienModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, version }, "Alien Mode", modeNames(), saucer));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { amountId, version }, "Alien Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, version }, "Alien Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, pctAttrs));
}

void AlienModule::attach (juce::AudioProcessorValueTreeState& state)
{
    modeChoice = state.getRawParameterValue (modeId);
    amountPct  = state.getRawParameterValue (amountId);
    mixPct     = state.getRawParameterValue (mixId);
    jassert (modeChoice != nullptr && amountPct != nullptr && mixPct != nullptr);
}

void AlienModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    const int size = juce::nextPowerOfTwo ((int) std::ceil (sampleRate / minRobotHz) + 4);
    combMask = size - 1;
    comb.assign ((size_t) juce::jmax (1u, spec.numChannels), std::vector<float> ((size_t) size, 0.0f));

    amount.reset (sampleRate, 0.05);
    mix.reset (sampleRate, 0.05);
    reset();
}

void AlienModule::reset()
{
    amount.setCurrentAndTargetValue (juce::jlimit (0.0f, 100.0f, amountPct->load()));
    mix.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));

    carrierPhase = 0.0;
    for (auto& c : comb)
        std::fill (c.begin(), c.end(), 0.0f);
    combWrite = 0;

    hp.fill (0.0f);
    lp1.fill (0.0f);
    lp2.fill (0.0f);
    crackle = 0.0f;
    fadePhase = 0.0;
    noise.setSeed (51); // same crackle every time: reproducible renders
}

void AlienModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    amount.setTargetValue (juce::jlimit (0.0f, 100.0f, amountPct->load()));
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));
    const int mode = juce::jlimit (0, 2, juce::roundToInt (modeChoice->load()));

    auto& block = context.getOutputBlock();
    const auto numChannels = juce::jmin (block.getNumChannels(), comb.size(), hp.size());
    const auto numSamples  = block.getNumSamples();
    const auto twoPi = juce::MathConstants<double>::twoPi;

    // Transmission band: 500 Hz .. 3 kHz.
    const float hpCoeff = onePoleCoeff (500.0f, sampleRate);
    const float lpCoeff = onePoleCoeff (juce::jmin (3000.0f, (float) (0.45 * sampleRate)), sampleRate);

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float a = amount.getNextValue();
        const float m = mix.getNextValue();
        const float a01 = a / 100.0f;

        // Per-sample values shared by both channels.
        float carrier = 0.0f, combDelay = 0.0f, fade = 1.0f;
        if (mode == saucer)
        {
            carrier = (float) std::sin (carrierPhase);
            carrierPhase += twoPi * ringFrequency (a) / sampleRate;
            if (carrierPhase > twoPi)
                carrierPhase -= twoPi;
        }
        else if (mode == android)
        {
            combDelay = (float) (sampleRate / robotFrequency (a));
        }
        else
        {
            // Slow fading in and out, deeper with Amount.
            fade = 1.0f - 0.4f * a01 * (0.5f + 0.5f * (float) std::sin (fadePhase));
            fadePhase += twoPi * 0.35 / sampleRate;
            if (fadePhase > twoPi)
                fadePhase -= twoPi;

            // Crackle: random pops that ring out briefly, more of them with Amount.
            if (noise.nextFloat() < 0.0015f * a01)
                crackle = (noise.nextFloat() * 2.0f - 1.0f) * 0.35f * a01;
            crackle *= 0.92f;
        }

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto& x = block.getChannelPointer (ch)[i];
            float wet = x;

            if (mode == saucer)
            {
                wet = x * carrier;
            }
            else if (mode == android)
            {
                auto& line = comb[ch];
                const float pos = (float) combWrite - combDelay;
                const float base = std::floor (pos);
                const float frac = pos - base;
                const int i0 = (int) base & combMask;
                const float delayed = line[(size_t) i0] + frac * (line[(size_t) ((i0 + 1) & combMask)] - line[(size_t) i0]);
                const float y = x + combFeedback * delayed;
                line[(size_t) combWrite] = y;
                wet = std::tanh (0.45f * y);
            }
            else
            {
                const float driven = std::tanh ((x + crackle) * (1.0f + 2.0f * a01)) / (1.0f + a01);
                hp[ch] += hpCoeff * (driven - hp[ch]);
                const float band = driven - hp[ch];
                lp1[ch] += lpCoeff * (band - lp1[ch]);
                lp2[ch] += lpCoeff * (lp1[ch] - lp2[ch]);
                wet = 1.6f * lp2[ch] * fade; // make up the level the band-pass takes away
            }

            x += m * (wet - x);
        }

        if (mode == android)
            combWrite = (combWrite + 1) & combMask;
    }

    for (auto* states : { &hp, &lp1, &lp2 })
        for (auto& s : *states)
            if (std::abs (s) < 1.0e-15f)
                s = 0.0f;
}
