#include "FxChain.h"
#include "FilterModule.h"
#include "DriveModule.h"
#include "ChorusModule.h"
#include "DelayModule.h"
#include "ReverbModule.h"
#include "PitchModule.h"
#include "LoFiModule.h"
#include "ChopModule.h"
#include "ReverseModule.h"

FxChain::FxChain()
{
    // Signal-chain order: shape and pitch the voice, add grit and texture,
    // widen, chop, then reverse swells, echoes and space.
    modules.push_back (std::make_unique<FilterModule>());
    modules.push_back (std::make_unique<PitchModule>());
    modules.push_back (std::make_unique<DriveModule>());
    modules.push_back (std::make_unique<LoFiModule>());
    modules.push_back (std::make_unique<ChorusModule>());
    modules.push_back (std::make_unique<ChopModule>());
    modules.push_back (std::make_unique<ReverseModule>());
    modules.push_back (std::make_unique<DelayModule>());
    modules.push_back (std::make_unique<ReverbModule>());

    // Parameter order: the order modules were released in. Append new modules
    // at the end; never reorder or remove one after release.
    parameterOrder = juce::StringArray { "filter", "drive", "chorus", "delay", "reverb", // v1
                                         "pitch", "lofi", "chop", "reverse" };            // v2
    jassert (parameterOrder.size() == (int) modules.size());

    fades.resize (modules.size());
}

void FxChain::addParameters (EffectModule::ParamList& params) const
{
    for (auto& id : parameterOrder)
    {
        const auto it = std::find_if (modules.begin(), modules.end(),
                                      [&] (const std::unique_ptr<EffectModule>& m) { return m->getId() == id; });
        jassert (it != modules.end());
        if (it == modules.end())
            continue;

        auto& m = *it;
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

void FxChain::setHostTempo (double bpm)
{
    for (auto& m : modules)
        m->setHostTempo (bpm);
}

void FxChain::setHostPosition (double ppq)
{
    for (auto& m : modules)
        m->setHostPosition (ppq);
}

void FxChain::process (juce::dsp::AudioBlock<float>& block)
{
    // Hosts may occasionally send more than the prepared block size. Modules
    // only ever see blocks up to that size (some JUCE effects need this).
    const auto chunkSize = (size_t) juce::jmax (1, scratch.getNumSamples());

    for (size_t start = 0; start < block.getNumSamples(); start += chunkSize)
    {
        auto chunk = block.getSubBlock (start, juce::jmin (chunkSize, block.getNumSamples() - start));
        processChunk (chunk);
    }
}

void FxChain::processChunk (juce::dsp::AudioBlock<float>& block)
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
