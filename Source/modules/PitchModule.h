#pragma once

#include "EffectModule.h"
#include "dsp/GrainShifter.h"

// ---------------------------------------------------------------------------
// PitchModule
//
// Shifts the vocal up or down by up to an octave (no formant correction, so
// up sounds small and bright, down sounds big and dark). Mix blends the
// shifted voice with the original with constant power: low Mix makes a
// pitched double or harmony under the vocal, 100 % replaces it.
// ---------------------------------------------------------------------------
class PitchModule : public EffectModule
{
public:
    static constexpr auto shiftId = "pitch_shift";
    static constexpr auto mixId   = "pitch_mix";

    static constexpr double windowSeconds = 0.05;

    juce::String getId() const override { return "pitch"; }
    juce::String getDisplayName() const override { return "Pitch"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

private:
    std::atomic<float>* semitones = nullptr;
    std::atomic<float>* mixPct    = nullptr;

    GrainShifter shifter;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> mix;
};
