#include "LoFiModule.h"

namespace
{
    // The wobble delay sways around depth * 1.2, so at Wobble 0 it is (almost)
    // no delay at all and the dry/crushed blend never combs.
    constexpr float maxWobbleMs   = 3.5f;
    constexpr float delayCentre   = 1.2f;
    constexpr float wowHz         = 0.9f;
    constexpr float flutterHz     = 6.5f;
    constexpr float flutterAmount = 0.15f; // relative to the wow depth
}

float LoFiModule::crushToBits (float percent)
{
    return 16.0f - 12.0f * juce::jlimit (0.0f, 100.0f, percent) / 100.0f;
}

float LoFiModule::crushToRate (float percent)
{
    const float c = juce::jlimit (0.0f, 100.0f, percent) / 100.0f;
    return 44100.0f * std::pow (0.1f, c); // 44.1 kHz .. 4.41 kHz, even steps by ear
}

void LoFiModule::addParameters (ParamList& params) const
{
    const auto pctAttrs = juce::AudioParameterFloatAttributes().withLabel ("%");
    const int version = getParameterVersion();

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crushId, version }, "Lo-Fi Crush",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { wobbleId, version }, "Lo-Fi Wobble",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pctAttrs));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, version }, "Lo-Fi Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttrs));
}

void LoFiModule::attach (juce::AudioProcessorValueTreeState& state)
{
    crushPct  = state.getRawParameterValue (crushId);
    wobblePct = state.getRawParameterValue (wobbleId);
    mixPct    = state.getRawParameterValue (mixId);
    jassert (crushPct != nullptr && wobblePct != nullptr && mixPct != nullptr);
}

void LoFiModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    wobbleLine.setMaximumDelayInSamples ((int) std::ceil (maxWobbleMs * (delayCentre + 1.0f + flutterAmount) * 0.001 * sampleRate) + 4);
    wobbleLine.prepare (spec);

    for (auto* s : { &crush, &wobble, &mix })
        s->reset (sampleRate, 0.05);

    reset();
}

void LoFiModule::reset()
{
    crush.setCurrentAndTargetValue (crushPct->load());
    wobble.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, wobblePct->load() / 100.0f));
    mix.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));

    wobbleLine.reset();
    held.fill (0.0f);
    lp1.fill (0.0f);
    lp2.fill (0.0f);
    holdPhase = 1.0f;
    wowPhase = flutterPhase = 0.0f;
}

void LoFiModule::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    crush.setTargetValue (juce::jlimit (0.0f, 100.0f, crushPct->load()));
    wobble.setTargetValue (juce::jlimit (0.0f, 1.0f, wobblePct->load() / 100.0f));
    mix.setTargetValue (juce::jlimit (0.0f, 1.0f, mixPct->load() / 100.0f));

    auto& block = context.getOutputBlock();
    const auto numChannels = juce::jmin (block.getNumChannels(), held.size());
    const auto numSamples  = block.getNumSamples();
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float msToSamples = (float) (0.001 * sampleRate);

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float c = crush.getNextValue();
        const float levels = std::pow (2.0f, crushToBits (c) - 1.0f);
        const float holdStep = juce::jmin (1.0f, crushToRate (c) / (float) sampleRate);
        // Darkening low-pass (two one-poles), 16 kHz clean .. about 2.5 kHz fully crushed.
        const float cutoff = juce::jmin (16000.0f * std::pow (0.16f, c / 100.0f), (float) (0.45 * sampleRate));
        const float lpCoeff = 1.0f - std::exp (-twoPi * cutoff / (float) sampleRate);

        // Tape wow (slow) + flutter (fast) as a swaying delay.
        const float depthMs = wobble.getNextValue() * maxWobbleMs;
        const float sway = std::sin (wowPhase) + flutterAmount * std::sin (flutterPhase);
        const float delay = depthMs * (delayCentre + sway) * msToSamples;
        wowPhase     += twoPi * wowHz / (float) sampleRate;
        flutterPhase += twoPi * flutterHz / (float) sampleRate;
        if (wowPhase > twoPi)     wowPhase -= twoPi;
        if (flutterPhase > twoPi) flutterPhase -= twoPi;

        // Sample-and-hold at the reduced rate.
        holdPhase += holdStep;
        const bool takeNew = holdPhase >= 1.0f;
        if (takeNew)
            holdPhase -= 1.0f;

        const float m = mix.getNextValue();

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto& x = block.getChannelPointer (ch)[i];

            // Wobble applies to the whole signal; Mix blends crushed vs clean.
            wobbleLine.pushSample ((int) ch, x);
            const float wobbled = wobbleLine.popSample ((int) ch, juce::jmax (0.0f, delay));

            if (takeNew)
                held[ch] = std::round (wobbled * levels) / levels;

            lp1[ch] += lpCoeff * (held[ch] - lp1[ch]);
            lp2[ch] += lpCoeff * (lp1[ch] - lp2[ch]);

            x = wobbled + m * (lp2[ch] - wobbled);
        }
    }

    for (auto* states : { &held, &lp1, &lp2 })
        for (auto& s : *states)
            if (std::abs (s) < 1.0e-15f)
                s = 0.0f;
}
