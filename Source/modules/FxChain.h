#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// FxChain
//
// Owns every EffectModule and runs the enabled ones in order.
// Each module automatically gets a "<id>_on" switch parameter.
//
// Phase 1: the chain is empty, so the plugin is a clean passthrough.
// Phase 2+: modules are registered in the constructor (FxChain.cpp).
// ---------------------------------------------------------------------------
class FxChain
{
public:
    FxChain();

    void addParameters (EffectModule::ParamList& params) const;
    void attach (juce::AudioProcessorValueTreeState& state);

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float>& block);

    int getLatencySamples() const;
    int getNumModules() const noexcept { return (int) modules.size(); }
    EffectModule& getModule (int index) { return *modules[(size_t) index]; }

    static juce::String enabledParamId (const juce::String& moduleId) { return moduleId + "_on"; }

private:
    std::vector<std::unique_ptr<EffectModule>> modules;
    std::vector<std::atomic<float>*> enabledFlags; // one per module, same order

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChain)
};
