#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "modules/FxChain.h"

// Signal flow:
//   input gain -> [dry tap] -> FxChain (enabled modules) -> dry/wet mix -> output gain
class DiceVoxProcessor : public juce::AudioProcessor
{
public:
    DiceVoxProcessor();
    ~DiceVoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock; // keep the double-precision overload visible

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // Declared before `state` on purpose: the parameter layout is built from it.
    FxChain chain;
    juce::AudioProcessorValueTreeState state;

private:
    std::atomic<float>* inputGainDb  = nullptr;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* mixPercent   = nullptr;
    std::atomic<float>* bypassFlag   = nullptr;

    juce::dsp::Gain<float> inputGain, outputGain;
    juce::dsp::DryWetMixer<float> dryWet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceVoxProcessor)
};
