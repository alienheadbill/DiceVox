#pragma once

#include "EffectModule.h"

#include <array>

// ---------------------------------------------------------------------------
// LoFiModule
//
// Dusty, phone-recorded, worn-tape character:
//   Crush   fewer bits (16 -> 4) and a lower sample rate (44.1 kHz -> about
//           4 kHz), then a darkening low-pass that follows it.
//   Wobble  tape wow and flutter: a slowly swaying pitch (modulated delay,
//           applied to the whole signal; at 0 there is no delay at all).
//   Mix     blend between the crushed and the clean (wobbled) vocal.
// Parameters are smoothed or applied per sample, so rolls don't click.
// ---------------------------------------------------------------------------
class LoFiModule : public EffectModule
{
public:
    static constexpr auto crushId  = "lofi_crush";
    static constexpr auto wobbleId = "lofi_wobble";
    static constexpr auto mixId    = "lofi_mix";

    // Crush 0..100 % -> bit depth and target sample rate.
    static float crushToBits (float percent);
    static float crushToRate (float percent);

    juce::String getId() const override { return "lofi"; }
    juce::String getDisplayName() const override { return "Lo-Fi"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

private:
    std::atomic<float>* crushPct  = nullptr;
    std::atomic<float>* wobblePct = nullptr;
    std::atomic<float>* mixPct    = nullptr;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> wobbleLine { 4 };
    juce::SmoothedValue<float> crush, wobble, mix;

    std::array<float, 2> held {}, lp1 {}, lp2 {};
    float holdPhase = 1.0f, wowPhase = 0.0f, flutterPhase = 0.0f;
    double sampleRate = 48000.0;
};
