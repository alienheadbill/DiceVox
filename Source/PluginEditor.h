#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ahe/AHELookAndFeel.h"
#include "ahe/AHEHeader.h"
#include "ahe/AHEKnob.h"
#include "ui/RollChamber.h"
#include "ui/EffectCard.h"

// ---------------------------------------------------------------------------
// DiceVox editor.
//
//   header
//   ROLL chamber      (ROLL / BACK, name of the current roll)
//   effect cards      (Filter, Drive, Chorus, Delay, Reverb: ON/OFF + knobs)
//   Bypass  ·  Input / Mix / Output
//
// Everything is laid out on an 800x530 design canvas ("content") that is
// scaled as a whole when the window is resized (100%-150%, fixed aspect
// ratio), so the layout never reflows and vector drawing stays crisp.
// ---------------------------------------------------------------------------
class DiceVoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit DiceVoxEditor (DiceVoxProcessor&);
    ~DiceVoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth  = 800;
    static constexpr int designHeight = 530;
    static constexpr double maxScale  = 1.5;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (DiceVoxProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        ahe::Header header { "DiceVox" };
        RollChamber roll;
        std::vector<std::unique_ptr<EffectCard>> cards;
        ahe::Knob inputKnob, mixKnob, outputKnob;
        juce::ToggleButton bypassSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
        juce::Rectangle<int> bottomArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
    };

    // Declared first so it outlives every component that uses it.
    ahe::LookAndFeel lookAndFeel;

    Content content;
    juce::TooltipWindow tooltips { this, 500 };
    ahe::KeyboardFocusIndicator focusIndicator { *this, lookAndFeel };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceVoxEditor)
};
