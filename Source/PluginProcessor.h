#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "modules/FxChain.h"
#include "presets/PresetStore.h"

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
    double getTailLengthSeconds() const override { return 10.0; } // reverb + delay repeats

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // ---- Dice (message thread only) ----------------------------------------
    // ROLL: a new random effect chain (see dice/DiceRoller.h). Returns its name.
    juce::String rollDice();
    juce::String rollDice (juce::Random& rng); // tests use a fixed seed

    // BACK: return the effects to how they were before the last roll or preset load.
    bool undoRoll();
    bool canUndoRoll() const noexcept { return ! rollHistory.empty(); }

    // Name of the current roll ("" before the first roll). Saved with the project.
    juce::String getRollName() const;

    // ---- Presets (message thread only) --------------------------------------
    // SAVE: writes the current sound as a preset; returns its file (or a
    // non-existent file if writing failed).
    juce::File savePreset (const juce::String& name);

    // Loads a preset's sound (BACK can undo it). False if it isn't a preset.
    bool loadPreset (const juce::File& file);

    // Every preset in menu order: the factory Alien Pack first, then the user's.
    struct PresetEntry
    {
        juce::String name;
        int factoryIndex = -1; // >= 0: built-in preset
        juce::File file;       // user preset file
        bool isFactory() const noexcept { return factoryIndex >= 0; }
    };
    std::vector<PresetEntry> getPresetList() const;
    bool loadPreset (const PresetEntry& entry);

    // Name of the loaded / saved preset; cleared by a roll. Saved with the project.
    juce::String getPresetName() const;
    bool isFactoryPresetLoaded() const;
    bool isCurrentPreset (const PresetEntry& entry) const;

    PresetStore presets;

    static constexpr auto rollNameProperty     = "rollName";
    static constexpr auto presetNameProperty   = "presetName";
    static constexpr auto presetFactoryProperty = "presetFactory";
    static constexpr size_t maxRollHistory = 32;

    // Declared before `state` on purpose: the parameter layout is built from it.
    FxChain chain;
    juce::AudioProcessorValueTreeState state;

private:
    void pushHistory();
    void applyPreset (const juce::XmlElement& xml, const juce::String& name, bool factory);

    std::vector<juce::ValueTree> rollHistory; // state before each roll / preset load, newest last
    juce::Random diceRandom;

    std::atomic<float>* inputGainDb  = nullptr;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* mixPercent   = nullptr;
    std::atomic<float>* bypassFlag   = nullptr;

    juce::dsp::Gain<float> inputGain, outputGain;
    juce::dsp::DryWetMixer<float> dryWet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceVoxProcessor)
};
