#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// FxChain
//
// Owns every EffectModule and runs the enabled ones in order.
// Each module automatically gets a "<id>_on" switch parameter (default off).
//
// Switching a module on or off crossfades between its input and output over
// a few milliseconds, so the switch never clicks. A module that is fully off
// is not processed at all (the signal passes through untouched), and its
// memory is cleared before it fades back in.
//
// Modules, in signal order (registered in FxChain.cpp):
//   filter -> drive -> chorus -> delay -> reverb
// ---------------------------------------------------------------------------
class FxChain
{
public:
    FxChain();

    void addParameters (EffectModule::ParamList& params) const;
    void attach (juce::AudioProcessorValueTreeState& state);

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setHostTempo (double bpm);
    void process (juce::dsp::AudioBlock<float>& block);

    int getLatencySamples() const;
    int getNumModules() const noexcept { return (int) modules.size(); }
    EffectModule& getModule (int index) { return *modules[(size_t) index]; }

    static juce::String enabledParamId (const juce::String& moduleId) { return moduleId + "_on"; }

    static constexpr double switchFadeSeconds = 0.01;

private:
    void processChunk (juce::dsp::AudioBlock<float>& block);
    void processCrossfading (size_t index, juce::dsp::AudioBlock<float>& block);

    std::vector<std::unique_ptr<EffectModule>> modules;
    std::vector<std::atomic<float>*> enabledFlags; // one per module, same order
    std::vector<juce::SmoothedValue<float>> fades;  // 0 = off, 1 = on, same order

    juce::AudioBuffer<float> scratch; // module output while crossfading

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChain)
};
