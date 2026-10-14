#include "FxChain.h"

FxChain::FxChain()
{
    // Phase 2 registers modules here, in signal-chain order, e.g.:
    //   modules.push_back (std::make_unique<FilterModule>());
    //   modules.push_back (std::make_unique<SaturationModule>());
    //   modules.push_back (std::make_unique<DelayModule>());
}

void FxChain::addParameters (EffectModule::ParamList& params) const
{
    for (auto& m : modules)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { enabledParamId (m->getId()), 1 },
            m->getDisplayName() + " On",
            false));

        m->addParameters (params);
    }
}

void FxChain::attach (juce::AudioProcessorValueTreeState& state)
{
    enabledFlags.clear();

    for (auto& m : modules)
    {
        auto* flag = state.getRawParameterValue (enabledParamId (m->getId()));
        jassert (flag != nullptr);
        enabledFlags.push_back (flag);
        m->attach (state);
    }
}

void FxChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    for (auto& m : modules)
        m->prepare (spec);
}

void FxChain::reset()
{
    for (auto& m : modules)
        m->reset();
}

void FxChain::process (juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> context (block);

    for (size_t i = 0; i < modules.size(); ++i)
        if (enabledFlags[i]->load() > 0.5f)
            modules[i]->process (context);
}

int FxChain::getLatencySamples() const
{
    // Latency is reported for every module (enabled or not) so the host's
    // delay compensation stays stable when the dice toggles modules on/off.
    int total = 0;
    for (auto& m : modules)
        total += m->getLatencySamples();
    return total;
}
