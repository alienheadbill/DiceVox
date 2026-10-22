#pragma once

#include "EffectModule.h"

#include <array>
#include <vector>

// ---------------------------------------------------------------------------
// AlienModule — "Alien Voice": the vocal as if beamed from a spaceship.
//
// Modes:
//   Saucer        ring modulator: the vocal times a sine carrier (20 Hz ..
//                 1.2 kHz). Low = flying-saucer wobble, high = metallic bells.
//   Android       robot voice: a resonant comb tuned to one pitch (60 ..
//                 240 Hz), so every word rings at the same metallic note.
//   Transmission  space radio: band-limited to 500 Hz - 3 kHz, slowly fading
//                 in and out, with crackle. Amount = how rough the signal is.
// Amount sets the character per mode; Mix blends with the clean vocal.
// Everything runs per sample (crackle from a seeded generator), so the sound
// doesn't depend on the host's block size. Adds no latency.
// ---------------------------------------------------------------------------
class AlienModule : public EffectModule
{
public:
    static constexpr auto modeId   = "alien_mode";
    static constexpr auto amountId = "alien_amount";
    static constexpr auto mixId    = "alien_mix";

    enum Mode { saucer, android, transmission };
    static const juce::StringArray& modeNames();

    static float ringFrequency (float amountPercent);  // Saucer carrier, Hz
    static float robotFrequency (float amountPercent); // Android comb pitch, Hz

    juce::String getId() const override { return "alien"; }
    juce::String getDisplayName() const override { return "Alien Voice"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

private:
    std::atomic<float>* modeChoice = nullptr;
    std::atomic<float>* amountPct  = nullptr;
    std::atomic<float>* mixPct     = nullptr;

    juce::SmoothedValue<float> amount, mix;
    double sampleRate = 48000.0;

    // Saucer
    double carrierPhase = 0.0;

    // Android: comb delay line per channel
    std::vector<std::vector<float>> comb;
    int combMask = 0, combWrite = 0;

    // Transmission: band-pass states, fade LFO, crackle
    std::array<float, 2> hp {}, lp1 {}, lp2 {};
    float crackle = 0.0f;
    double fadePhase = 0.0;
    juce::Random noise;
};
