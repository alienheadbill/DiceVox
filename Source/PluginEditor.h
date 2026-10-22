#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ahe/AHELookAndFeel.h"
#include "ahe/AHEHeader.h"
#include "ahe/AHEKnob.h"
#include "ui/RollChamber.h"
#include "ui/EffectCard.h"
#include "ui/PresetBar.h"

// ---------------------------------------------------------------------------
// DiceVox editor.
//
//   header            (preset bar: < preset menu > SAVE)
//   ROLL chamber      (ROLL / BACK, style, intensity, name of the current roll)
//   effect cards      (two rows, signal order: ON/OFF, lock, knobs)
//   Bypass  ·  Input / Mix / Output
//
// Everything is laid out on an 860x690 design canvas ("content") that is
// scaled as a whole when the window is resized (80%-130%, fixed aspect
// ratio), so the layout never reflows and vector drawing stays crisp.
// ---------------------------------------------------------------------------
class DiceVoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit DiceVoxEditor (DiceVoxProcessor&);
    ~DiceVoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth  = 860;
    static constexpr int designHeight = 690;
    static constexpr double minScale  = 0.8;
    static constexpr double maxScale  = 1.3;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (DiceVoxProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        ahe::Header header { "DiceVox" };
        PresetBar presetBar;
        RollChamber roll;
        std::vector<std::unique_ptr<EffectCard>> cards;
        ahe::Knob inputKnob, mixKnob, outputKnob;
        juce::ToggleButton bypassSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
        juce::Rectangle<int> bottomArea;

        struct Star { float x, y, size, alpha; };
        std::vector<Star> stars; // background starfield, generated once

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
    };

    // Declared first so it outlives every component that uses it.
    ahe::LookAndFeel lookAndFeel;

    Content content;
    juce::TooltipWindow tooltips { this, 500 };
    ahe::KeyboardFocusIndicator focusIndicator { *this, lookAndFeel };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceVoxEditor)
};
