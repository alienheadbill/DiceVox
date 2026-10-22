#pragma once

#include "ahe/AHETheme.h"

class DiceVoxProcessor;

// ---------------------------------------------------------------------------
// PresetBar — the preset controls in the header.
//
//   [<]  [ My Dreamy Hook          v ]  [>]  [SAVE]
//
// The name button opens a menu of the user's presets (Documents/AHE Audio/
// DiceVox/Presets) plus "Save as new preset..." and "Open presets folder".
// < and > step through the presets. SAVE asks for a name (suggesting the
// current preset or roll) and writes the current sound.
// ---------------------------------------------------------------------------
class PresetBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit PresetBar (DiceVoxProcessor&);
    ~PresetBar() override;

    void resized() override;

    // Loads the preset `offset` steps away from the current one (wraps around).
    void step (int offset);

    // Saves without asking (the dialog calls this); returns false on failure.
    bool saveAs (const juce::String& name);

    juce::TextButton& getSaveButton() noexcept { return saveButton; }
    juce::TextButton& getNameButton() noexcept { return nameButton; }

    static constexpr auto noPresetText = "No preset";

private:
    void timerCallback() override;
    void refresh();
    void showMenu();
    void askForName();

    DiceVoxProcessor& processor;
    juce::TextButton previousButton { "<" }, nameButton, nextButton { ">" }, saveButton { "SAVE" };
    juce::Component::SafePointer<juce::AlertWindow> dialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
