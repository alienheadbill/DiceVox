#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// FilterModule (Phase 2A)
//
// Low cut (high-pass) followed by high cut (low-pass), both 12 dB/oct
// state-variable filters (TPT form, which stays stable and click-free while
// the cutoff moves). One shared resonance control.
//
// Vocal uses: rumble clean-up (default 80 Hz low cut), telephone / radio
// band-pass, muffled "through the wall", thin and tinny.
//
// Cutoffs and resonance are smoothed per sample, so automation and (Phase 3)
// randomizer jumps don't click. Adds no latency.
// ---------------------------------------------------------------------------
class FilterModule : public EffectModule
{
public:
    static constexpr auto lowCutId    = "filter_lowcut";
    static constexpr auto highCutId   = "filter_highcut";
    static constexpr auto resonanceId = "filter_resonance";

    // Resonance 0..100 % maps to Q 0.707 (flat Butterworth, no peak) .. maxQ
    // (about +8 dB peak at each cutoff), so a full-resonance roll stays safe.
    static constexpr float minQ = 0.70710678f;
    static constexpr float maxQ = 2.5f;
    static float resonanceToQ (float percent);

    juce::String getId() const override { return "filter"; }
    juce::String getDisplayName() const override { return "Filter"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

private:
    void setTargetsFromParameters();
    void applyCoefficients (float lowCutHz, float highCutHz, float q);
    float clampCutoff (float hz) const noexcept;

    std::atomic<float>* lowCutHz     = nullptr;
    std::atomic<float>* highCutHz    = nullptr;
    std::atomic<float>* resonancePct = nullptr;

    juce::dsp::StateVariableTPTFilter<float> lowCut, highCut;

    // Cutoffs smooth on a log scale (even speed across octaves), Q linearly.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowCutSmoothed, highCutSmoothed;
    juce::SmoothedValue<float> qSmoothed;

    double sampleRate = 48000.0;
};
