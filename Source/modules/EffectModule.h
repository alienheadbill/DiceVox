#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ---------------------------------------------------------------------------
// EffectModule
//
// Every effect in DiceVox (pitch, doubler, reverb, delay, ...) is one class
// that implements this interface. The FxChain owns the modules, gives each one
// an on/off switch, and runs the enabled ones in order.
//
// Rules for a module:
//  - Parameter IDs must start with getId() + "_"  (e.g. "delay_time").
//  - process() runs on the audio thread: no allocation, no locks.
//  - Randomizing is NOT done here. The randomizer engine (Phase 3) works on
//    parameter IDs and ranges, so modules stay pure DSP.
// ---------------------------------------------------------------------------
class EffectModule
{
public:
    using ParamList = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;

    virtual ~EffectModule() = default;

    // Short lowercase id used as the parameter prefix, e.g. "delay".
    virtual juce::String getId() const = 0;

    // Display name shown in the edit panel, e.g. "Delay".
    virtual juce::String getDisplayName() const = 0;

    // juce::ParameterID version hint for this module's parameters (and its
    // "<id>_on" switch): the release that first shipped the module. Phase 1
    // parameters are 1; Phase 2 modules use 2. Never lower it after release.
    virtual int getParameterVersion() const = 0;

    // Add this module's parameters (not including its on/off switch).
    virtual void addParameters (ParamList& params) const = 0;

    // Grab pointers to this module's parameters once the state tree exists.
    virtual void attach (juce::AudioProcessorValueTreeState& state) = 0;

    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process (const juce::dsp::ProcessContextReplacing<float>& context) = 0;

    // Extra latency this module adds, in samples (pitch shifters need this).
    virtual int getLatencySamples() const { return 0; }
};
