#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"
#include "dice/DiceRoller.h"

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

    // Tempo-synced effects follow the host (they keep 120 BPM if it sends none).
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                chain.setHostTempo (*bpm);

            // Beat-locked effects (Chop) follow the song position while playing.
            if (position->getIsPlaying())
                if (const auto ppq = position->getPpqPosition())
                    chain.setHostPosition (*ppq);
        }
    }

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
    {
        if (xml->hasTagName (state.state.getType()))
        {
            state.replaceState (juce::ValueTree::fromXml (*xml));
            rollHistory.clear(); // BACK never jumps to a different project's sound
        }
    }
}

// ============================================================================
// Dice
// ============================================================================
juce::String DiceVoxProcessor::rollDice()
{
    return rollDice (diceRandom);
}

juce::String DiceVoxProcessor::rollDice (juce::Random& rng)
{
    rollHistory.push_back (state.copyState());
    if (rollHistory.size() > maxRollHistory)
        rollHistory.erase (rollHistory.begin());

    const auto name = DiceRoller::roll (state, rng);
    state.state.setProperty (rollNameProperty, name, nullptr);
    return name;
}

bool DiceVoxProcessor::undoRoll()
{
    if (rollHistory.empty())
        return false;

    const auto snapshot = rollHistory.back();
    rollHistory.pop_back();

    // Restore every effect parameter. Input, Mix, Output, Bypass and the dice
    // controls (intensity, style, locks) are left alone: the dice never
    // changes them, so BACK shouldn't either.
    const juce::StringArray globals { ParamIDs::inputGain, ParamIDs::outputGain, ParamIDs::mix, ParamIDs::bypass };

    for (auto* param : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr || globals.contains (ranged->getParameterID()) || ParamIDs::isDiceControl (ranged->getParameterID()))
            continue;

        const auto saved = snapshot.getChildWithProperty ("id", ranged->getParameterID());
        if (! saved.isValid())
            continue;

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (ranged->convertTo0to1 ((float) saved.getProperty ("value")));
        ranged->endChangeGesture();
    }

    state.state.setProperty (rollNameProperty, snapshot.getProperty (rollNameProperty), nullptr);
    return true;
}

juce::String DiceVoxProcessor::getRollName() const
{
    return state.state.getProperty (rollNameProperty).toString();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DiceVoxProcessor();
}
