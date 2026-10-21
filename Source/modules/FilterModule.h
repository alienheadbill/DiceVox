#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// FilterModule (Phase 2A)
//
// Low cut (high-pass) followed by high cut (low-pass), both 12 dB/oct
// state-variable filters (TPT form, which stays stable and click-free while
// the cutoff moves). One shared resonance control.
//
// Resonance safety: when the two cutoffs are close, both resonant peaks land
// on the same frequencies and multiply (Q x Q, up to +16 dB). Each stage's Q
// is therefore reduced as the cutoffs converge (FilterQTable.h), so the
// combined peak never exceeds one stage's peak at the dialled resonance
// (about +8 dB at 100 %). Cutoffs more than ~4 octaves apart keep full Q.
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
    // (about +8 dB peak), so a full-resonance roll stays safe.
    // Keep in sync with tools/dsp/gen-filter-q-table.py.
    static constexpr float minQ = 0.70710678f;
    static constexpr float maxQ = 2.5f;
    static float resonanceToQ (float percent);

    // The Q each stage actually uses: the dialled Q, reduced as the cutoffs
    // converge so the combined peak stays within one stage's peak.
    static float stageQ (float resonancePercent, float lowCutHz, float highCutHz, double sampleRate);

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
    void applyCoefficients (float lowCutHz, float highCutHz, float resonancePercent);
    float clampCutoff (float hz) const noexcept;

    std::atomic<float>* lowCutHz     = nullptr;
    std::atomic<float>* highCutHz    = nullptr;
    std::atomic<float>* resonancePct = nullptr;

    juce::dsp::StateVariableTPTFilter<float> lowCut, highCut;

    // Cutoffs smooth on a log scale (even speed across octaves), resonance
    // linearly. The per-stage Q follows them, so the safety limit also glides.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowCutSmoothed, highCutSmoothed;
    juce::SmoothedValue<float> resonanceSmoothed;

    double sampleRate = 48000.0;
};
