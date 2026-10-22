#pragma once

#include "ahe/AHEKnob.h"

// ---------------------------------------------------------------------------
// EffectCard — one effect of the chain on screen.
//
//   DELAY        (o ON)
//   ( knob )  ( knob )
//        ( knob )
//
// The ON / OFF switch is the module's "<id>_on" parameter; the knobs are its
// parameters (two per row). The card outline lights up while the effect is on.
// Every control has the standard APVTS attachment, so automation, host undo
// and dice rolls all show up here.
// ---------------------------------------------------------------------------
class EffectCard : public juce::Component
{
public:
    struct KnobSpec
    {
        juce::String paramId, name, tooltip;
    };

    EffectCard (juce::AudioProcessorValueTreeState& state, const juce::String& moduleId,
                const juce::String& title, const juce::String& description,
                const std::vector<KnobSpec>& knobs);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<ahe::Knob>> knobs;
    juce::Rectangle<int> titleArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectCard)
};
