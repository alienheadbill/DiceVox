#include "GrainShifter.h"

void GrainShifter::prepare (double newSampleRate, int numChannels, double maxWindowSeconds)
{
    sampleRate = newSampleRate;
    maxWindow  = (float) (maxWindowSeconds * sampleRate);

    const int size = juce::nextPowerOfTwo ((int) std::ceil (maxWindow) + 8);
    mask = size - 1;
    buffers.assign ((size_t) juce::jmax (1, numChannels), std::vector<float> ((size_t) size, 0.0f));

    window.reset (sampleRate, 0.1);
    window.setCurrentAndTargetValue (juce::jlimit (16.0f, maxWindow, (float) (0.05 * sampleRate)));
    reset();
}

void GrainShifter::reset()
{
    for (auto& b : buffers)
        std::fill (b.begin(), b.end(), 0.0f);

    writePos = 0;
    phase = 0.0f;
    window.setCurrentAndTargetValue (window.getTargetValue());
}

void GrainShifter::setWindowSeconds (double seconds) noexcept
{
    window.setTargetValue (juce::jlimit (16.0f, maxWindow, (float) (seconds * sampleRate)));
}

float GrainShifter::readAt (const std::vector<float>& buffer, float delay) const noexcept
{
    const float pos = (float) writePos - delay;
    const float base = std::floor (pos);
    const float frac = pos - base;
    const int i0 = (int) base & mask;
    const int i1 = (i0 + 1) & mask;
    return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
}

void GrainShifter::process (const juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = juce::jmin (block.getNumChannels(), buffers.size());
    const auto numSamples  = block.getNumSamples();

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float w = window.getNextValue();

        // Head A at `phase`, head B half a window later; their gains sum to 1.
        const float phaseB = phase + 0.5f - std::floor (phase + 0.5f);
        const float delayA = 1.0f + phase * w;
        const float delayB = 1.0f + phaseB * w;
        const float s = std::sin (juce::MathConstants<float>::pi * phase);
        const float gainA = s * s;

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto& buffer = buffers[ch];
            auto& sample = block.getChannelPointer (ch)[i];
            buffer[(size_t) writePos] = sample;
            sample = gainA * readAt (buffer, delayA) + (1.0f - gainA) * readAt (buffer, delayB);
        }

        writePos = (writePos + 1) & mask;

        // The delay changes by (1 - ratio) samples per sample, so the heads
        // play back at `ratio` times the original speed.
        phase += (1.0f - ratio) / w;
        phase -= std::floor (phase);
    }
}
