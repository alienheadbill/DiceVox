#pragma once

#include "EffectModule.h"

#include <array>

// ---------------------------------------------------------------------------
// DriveModule
//
// Soft saturation (tanh) for warmth and grit, followed by a Tone control
// (a gentle low-pass, 2 kHz .. 20 kHz) that tames the fizz.
//
// Level: the output is scaled so a typical vocal peak (about -12 dBFS) keeps
// its level at any Amount. Louder peaks get rounded off and quieter parts come
// up, like a compressor. Turning Amount up adds character, not volume.
//
// Amount and Tone are smoothed per sample, so automation and dice rolls don't
// click. Adds no latency.
// ---------------------------------------------------------------------------
class DriveModule : public EffectModule
{
public:
    static constexpr auto amountId = "drive_amount";
    static constexpr auto toneId   = "drive_tone";

    // Amount 0..100 % -> 0..+24 dB of gain into the saturator.
    static float amountToGain (float percent);
    // Tone 0..100 % -> low-pass cutoff 2 kHz (dark) .. 20 kHz (open).
    static float toneToCutoff (float percent);

    juce::String getId() const override { return "drive"; }
    juce::String getDisplayName() const override { return "Drive"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

private:
    void setTargetsFromParameters();

    std::atomic<float>* amountPct = nullptr;
    std::atomic<float>* tonePct   = nullptr;

    juce::SmoothedValue<float> gain;   // linear drive gain
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff; // Hz

    std::array<float, 2> toneState {}; // one-pole low-pass memory, per channel
    double sampleRate = 48000.0;
};
