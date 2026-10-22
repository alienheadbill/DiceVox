#pragma once

#include "EffectModule.h"

#include <array>

// ---------------------------------------------------------------------------
// DelayModule
//
// Tempo-synced ping-pong echo. The vocal feeds mostly the left echo line and
// each line feeds the other, so repeats bounce left / right (mono tracks get
// a plain echo). The repeats are filtered (low cut 150 Hz, high cut 5 kHz) so
// they sit behind the vocal and get darker as they fade, and soft-clipped so
// high feedback can never run away.
//
// Time follows the host tempo (120 BPM when the host doesn't send one).
// Time changes glide over 250 ms (a short tape-style pitch bend) instead of
// clicking. The dry vocal is untouched: Mix only sets the echo level.
// Adds no latency.
// ---------------------------------------------------------------------------
class DelayModule : public EffectModule
{
public:
    static constexpr auto timeId     = "delay_time";
    static constexpr auto feedbackId = "delay_feedback";
    static constexpr auto mixId      = "delay_mix";

    // Note divisions offered by the Time control, and their length in beats.
    static const juce::StringArray& divisionNames();
    static double divisionBeats (int index);
    static constexpr int defaultDivision = 5; // 1/4

    static constexpr double maxDelaySeconds = 3.0;
    static constexpr float  maxFeedback     = 85.0f; // %

    // Echo length in seconds for a division at a tempo (clamped to maxDelaySeconds).
    static double delaySeconds (int divisionIndex, double bpm);

    juce::String getId() const override { return "delay"; }
    juce::String getDisplayName() const override { return "Delay"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

    void setHostTempo (double bpm) override;

private:
    void setTargetsFromParameters();
    float filterRepeat (size_t channel, float x) noexcept;

    std::atomic<float>* division    = nullptr;
    std::atomic<float>* feedbackPct = nullptr;
    std::atomic<float>* mixPct      = nullptr;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line { 4 };
    juce::SmoothedValue<float> delaySamples, feedback, wet;

    std::array<float, 2> lowState {}, highState {}; // repeat filters, per channel
    float lowCoeff = 0.0f, highCoeff = 0.0f;

    double sampleRate = 48000.0;
    double tempo = 120.0;
};
