#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Phase 1 UI: Input / Mix / Output knobs, bypass, and a placeholder dice
// button (wired up in Phase 3). The full edit panel arrives in Phase 5.
class DiceVoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit DiceVoxEditor (DiceVoxProcessor&);
    ~DiceVoxEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct LabelledKnob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setUpKnob (LabelledKnob& knob, const juce::String& paramId, const juce::String& name);

    DiceVoxProcessor& processor;

    LabelledKnob inputKnob, mixKnob, outputKnob;

    juce::TextButton diceButton { "ROLL" };
    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceVoxEditor)
};
