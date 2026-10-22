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

float AlienModule::transmissionLowCut (float amountPercent)
{
    return 300.0f * std::pow (700.0f / 300.0f, juce::jlimit (0.0f, 100.0f, amountPercent) / 100.0f); // 300 .. 700 Hz
}

float AlienModule::transmissionHighCut (float amountPercent)
{
    return 4000.0f * std::pow (2200.0f / 4000.0f, juce::jlimit (0.0f, 100.0f, amountPercent) / 100.0f); // 4 .. 2.2 kHz
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

    hpA.fill (0.0f);
    hpB.fill (0.0f);
    lp1.fill (0.0f);
    lp2.fill (0.0f);
    flutterPhase = 0.0;
    fadePhase = 0.0;
}

void AlienModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    amount.setTargetValue (juce::jlimit (0.0f, 100.0f, amountPct->load()));
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));
    const int mode = juce::jlimit (0, 2, juce::roundToInt (modeChoice->load()));

    auto& block = context.getOutputBlock();
    const auto numChannels = juce::jmin (block.getNumChannels(), comb.size(), hpA.size());
    const auto numSamples  = block.getNumSamples();
    const auto twoPi = juce::MathConstants<double>::twoPi;

    // Transmission band edges move with Amount (computed per sample while it glides).
    float hpCoeff = 0.0f, lpCoeff = 0.0f;
    auto updateBand = [&] (float a01)
    {
        hpCoeff = onePoleCoeff (transmissionLowCut (a01 * 100.0f), sampleRate);
        lpCoeff = onePoleCoeff (juce::jmin (transmissionHighCut (a01 * 100.0f), (float) (0.45 * sampleRate)), sampleRate);
    };
    updateBand (amount.getCurrentValue() / 100.0f);
    const bool amountMoving = amount.isSmoothing();

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
            if (amountMoving)
                updateBand (a01);

            // A distant signal: a slow fade in and out plus a light flutter,
            // both deeper with Amount. Never any added noise.
            fade = 1.0f - a01 * (0.25f * (0.5f + 0.5f * (float) std::sin (fadePhase))
                                 + 0.12f * (0.5f + 0.5f * (float) std::sin (flutterPhase)));
            fadePhase += twoPi * 0.3 / sampleRate;
            flutterPhase += twoPi * 5.5 / sampleRate;
            if (fadePhase > twoPi)    fadePhase -= twoPi;
            if (flutterPhase > twoPi) flutterPhase -= twoPi;
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
                // Gentle saturation (unity for quiet signals, peaks rounded),
                // then a steep radio band: two high-pass and two low-pass stages.
                const float g = 1.0f + 1.5f * a01;
                const float driven = std::tanh (g * x) / g;
                hpA[ch] += hpCoeff * (driven - hpA[ch]);
                const float hp1 = driven - hpA[ch];
                hpB[ch] += hpCoeff * (hp1 - hpB[ch]);
                const float hp2 = hp1 - hpB[ch];
                lp1[ch] += lpCoeff * (hp2 - lp1[ch]);
                lp2[ch] += lpCoeff * (lp1[ch] - lp2[ch]);
                wet = 1.4f * lp2[ch] * fade; // make up the level the band takes away
            }

            x += m * (wet - x);
        }

        if (mode == android)
            combWrite = (combWrite + 1) & combMask;
    }

    for (auto* states : { &hpA, &hpB, &lp1, &lp2 })
        for (auto& s : *states)
            if (std::abs (s) < 1.0e-15f)
                s = 0.0f;
}
