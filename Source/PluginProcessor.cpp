#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

DiceVoxProcessor::DiceVoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "DiceVoxState", Params::createLayout (chain))
{
    inputGainDb  = state.getRawParameterValue (ParamIDs::inputGain);
    outputGainDb = state.getRawParameterValue (ParamIDs::outputGain);
    mixPercent   = state.getRawParameterValue (ParamIDs::mix);
    bypassFlag   = state.getRawParameterValue (ParamIDs::bypass);

    chain.attach (state);
}

void DiceVoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        (juce::uint32) samplesPerBlock,
                                        (juce::uint32) getTotalNumOutputChannels() };

    // Set targets BEFORE prepare(): prepare() snaps the smoothers to their target,
    // so playback starts at the right level instead of fading in from silence.
    inputGain.setGainDecibels  (inputGainDb->load());
    outputGain.setGainDecibels (outputGainDb->load());
    dryWet.setWetMixProportion (mixPercent->load() / 100.0f);

    for (auto* g : { &inputGain, &outputGain })
    {
        g->setRampDurationSeconds (0.02); // smooth knob moves, no zipper noise
        g->prepare (spec);
    }

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    chain.prepare (spec);

    const int latency = chain.getLatencySamples();
    dryWet.setWetLatency ((float) latency);
    setLatencySamples (latency);
}

void DiceVoxProcessor::releaseResources()
{
    chain.reset();
}

bool DiceVoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void DiceVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (bypassFlag->load() > 0.5f)
        return; // TODO (Phase 2+): delay the dry signal by the chain latency while bypassed

    inputGain.setGainDecibels  (inputGainDb->load());
    outputGain.setGainDecibels (outputGainDb->load());
    dryWet.setWetMixProportion (mixPercent->load() / 100.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    inputGain.process (context);
    dryWet.pushDrySamples (block);
    chain.process (block);
    dryWet.mixWetSamples (block);
    outputGain.process (context);
}

juce::AudioProcessorEditor* DiceVoxProcessor::createEditor()
{
    return new DiceVoxEditor (*this);
}

juce::AudioProcessorParameter* DiceVoxProcessor::getBypassParameter() const
{
    return state.getParameter (ParamIDs::bypass);
}

void DiceVoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DiceVoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DiceVoxProcessor();
}
