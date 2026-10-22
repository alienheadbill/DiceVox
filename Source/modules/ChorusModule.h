#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// ChorusModule
//
// Doubler / chorus for width. Two JUCE choruses, one per channel, with the
// right one running slightly faster, so the left and right copies drift apart
// and the vocal spreads across the stereo field. Equal dry/wet blend.
//
// Rate is the wobble speed, Depth how far the copies drift. Adds no latency.
// ---------------------------------------------------------------------------
class ChorusModule : public EffectModule
{
public:
    static constexpr auto rateId  = "chorus_rate";
    static constexpr auto depthId = "chorus_depth";

    juce::String getId() const override { return "chorus"; }
    juce::String getDisplayName() const override { return "Chorus"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

    // The right channel's rate, relative to the left (makes the stereo spread).
    static constexpr float rightRateRatio = 1.13f;

private:
    void applyParameters();

    std::atomic<float>* rateHz   = nullptr;
    std::atomic<float>* depthPct = nullptr;

    juce::dsp::Chorus<float> left, right;
};
