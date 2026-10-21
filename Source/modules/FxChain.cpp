#include "FxChain.h"
#include "FilterModule.h"

FxChain::FxChain()
{
    // Signal-chain order. Phase 2B/2C add saturation, then delay, after the filter.
    modules.push_back (std::make_unique<FilterModule>());

    fades.resize (modules.size());
}

void FxChain::addParameters (EffectModule::ParamList& params) const
{
    for (auto& m : modules)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { enabledParamId (m->getId()), m->getParameterVersion() },
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
    scratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, true, false);

    for (size_t i = 0; i < modules.size(); ++i)
    {
        modules[i]->prepare (spec);

        // Start in the saved on/off state, no fade on load.
        fades[i].reset (spec.sampleRate, switchFadeSeconds);
        fades[i].setCurrentAndTargetValue (enabledFlags[i]->load() > 0.5f ? 1.0f : 0.0f);
    }
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
    {
        auto& fade = fades[i];
        const bool on = enabledFlags[i]->load() > 0.5f;

        if (on != (fade.getTargetValue() > 0.5f))
        {
            // Turning on from fully off: clear stale memory from the last time it ran.
            if (on && fade.getCurrentValue() <= 0.0f)
                modules[i]->reset();

            fade.setTargetValue (on ? 1.0f : 0.0f);
        }

        if (fade.isSmoothing())
            processCrossfading (i, block);
        else if (fade.getCurrentValue() > 0.5f)
            modules[i]->process (context);
        // else fully off: untouched
    }
}

void FxChain::processCrossfading (size_t index, juce::dsp::AudioBlock<float>& block)
{
    auto& fade = fades[index];
    const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) scratch.getNumChannels());
    const auto chunkSize   = (size_t) scratch.getNumSamples();

    // Hosts may occasionally send more than the prepared block size: work in chunks.
    for (size_t start = 0; start < block.getNumSamples(); start += chunkSize)
    {
        const auto len = juce::jmin (chunkSize, block.getNumSamples() - start);
        auto dry = block.getSubBlock (start, len);
        auto wet = juce::dsp::AudioBlock<float> (scratch).getSubsetChannelBlock (0, numChannels).getSubBlock (0, len);

        wet.copyFrom (dry);
        modules[index]->process (juce::dsp::ProcessContextReplacing<float> (wet));

        for (size_t n = 0; n < len; ++n)
        {
            const float g = fade.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& d = dry.getChannelPointer (ch)[n];
                d += g * (wet.getChannelPointer (ch)[n] - d);
            }
        }
    }
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
