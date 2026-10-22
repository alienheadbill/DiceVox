#pragma once

#include "EffectModule.h"
#include "dsp/GrainShifter.h"

// ---------------------------------------------------------------------------
// ReverseModule
//
// Reverse echo: chops the vocal into tempo-synced slices and plays each one
// backwards, a slice behind the original, so words swell in backwards before
// and around the dry vocal. Length is the slice in notes (host tempo, 120 BPM
// when none); Mix sets the reversed level (the dry vocal stays at full level).
// ---------------------------------------------------------------------------
class ReverseModule : public EffectModule
{
public:
    static constexpr auto lengthId = "reverse_length";
    static constexpr auto mixId    = "reverse_mix";

    static const juce::StringArray& lengthNames();
    static double lengthBeats (int index);
    static constexpr int defaultLength = 2; // 1/4
    static constexpr double maxSliceSeconds = 1.5;

    // Slice length in seconds for a choice at a tempo (clamped to maxSliceSeconds).
    static double sliceSeconds (int lengthIndex, double bpm);

    juce::String getId() const override { return "reverse"; }
    juce::String getDisplayName() const override { return "Reverse"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

    void setHostTempo (double bpm) override;

private:
    void updateWindow();

    std::atomic<float>* lengthChoice = nullptr;
    std::atomic<float>* mixPct       = nullptr;

    GrainShifter shifter;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> mix;
    double tempo = 120.0;
};
