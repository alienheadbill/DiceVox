#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

// ---------------------------------------------------------------------------
// GrainShifter — delay-line pitch shifter with two crossfaded read heads.
//
// Each head reads the recent input through a delay that sweeps across one
// window; the heads are half a window apart and fade in and out with
// complementary Hann windows (sin^2 + cos^2 = 1), so the output never steps.
//
//   ratio 2.0   one octave up         ratio 0.5   one octave down
//   ratio -1.0  reverse: each grain plays the last half-window backwards
//
// Output is wet only (the caller blends it with the dry signal). The shifted
// sound trails the input by about half a window; no latency is reported, as
// the effect is used as a double / texture next to the dry vocal.
// ---------------------------------------------------------------------------
class GrainShifter
{
public:
    void prepare (double sampleRate, int numChannels, double maxWindowSeconds);
    void reset();

    void setRatio (float newRatio) noexcept { ratio = newRatio; }

    // Window length; changes glide over 100 ms so the heads never jump.
    void setWindowSeconds (double seconds) noexcept;

    // Replaces the block with the shifted signal.
    void process (const juce::dsp::AudioBlock<float>& block) noexcept;

private:
    float readAt (const std::vector<float>& buffer, float delay) const noexcept;

    std::vector<std::vector<float>> buffers;
    int mask = 0, writePos = 0;
    float phase = 0.0f, ratio = 1.0f, maxWindow = 1.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> window; // samples
    double sampleRate = 48000.0;
};
