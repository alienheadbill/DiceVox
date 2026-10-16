#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ahe/AHELookAndFeel.h"
#include "ahe/AHEHeader.h"
#include "ahe/AHEKnob.h"
#include "ui/RollChamber.h"

// ---------------------------------------------------------------------------
// DiceVox editor — UI milestone 01 (AHE Audio visual shell).
//
// Interactive now:  Input / Mix / Output knobs, Bypass switch (same APVTS
//                   attachments and parameter IDs as Phase 1), QUICK tab.
// Shown, disabled:  ROLL chamber (Phase 3), ADVANCED tab (Phase 5).
//
// Everything is laid out on a 520x360 design canvas ("content") that is scaled
// as a whole when the window is resized (100%-150%, fixed aspect ratio), so the
// layout never reflows and vector drawing stays crisp at every size.
// ---------------------------------------------------------------------------
class DiceVoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit DiceVoxEditor (DiceVoxProcessor&);
    ~DiceVoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth  = 520;
    static constexpr int designHeight = 360;
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
        juce::TextButton quickTab { "QUICK" }, advancedTab { "ADVANCED" };
        RollChamber roll;
        ahe::Knob inputKnob, mixKnob, outputKnob;
        juce::ToggleButton bypassSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
        juce::Rectangle<int> footerArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
    };

    // Declared first so it outlives every component that uses it.
    ahe::LookAndFeel lookAndFeel;

    Content content;
    juce::TooltipWindow tooltips { this, 500 };
    ahe::KeyboardFocusIndicator focusIndicator { *this, lookAndFeel };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceVoxEditor)
};
