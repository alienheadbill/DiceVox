#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// ReverbModule
//
// JUCE's Freeverb-style reverb. Size goes from a small room to a long hall;
// Mix adds the reverb on top of the vocal (the dry vocal only dips slightly
// at high Mix, so the vocal stays upfront). Parameter changes are smoothed
// inside the reverb, so dice rolls don't click. Adds no latency.
// ---------------------------------------------------------------------------
class ReverbModule : public EffectModule
{
public:
    static constexpr auto sizeId = "reverb_size";
    static constexpr auto mixId  = "reverb_mix";

    static juce::Reverb::Parameters makeParameters (float sizePercent, float mixPercent);

    juce::String getId() const override { return "reverb"; }
    juce::String getDisplayName() const override { return "Reverb"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

private:
    void applyParameters();

    std::atomic<float>* sizePct = nullptr;
    std::atomic<float>* mixPct  = nullptr;
    float lastSize = -1.0f, lastMix = -1.0f;

    juce::dsp::Reverb reverb;
};
