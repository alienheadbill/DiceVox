#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ahe/AHETheme.h"

// ---------------------------------------------------------------------------
// LevelSelector — a row of segment buttons for one choice parameter,
// e.g. the dice intensity:   [ LOW | MED | HIGH ]
//
// Exactly one segment is lit. Clicking one sets the parameter (as a complete
// host gesture); host automation and project recall light the right one.
// ---------------------------------------------------------------------------
class LevelSelector : public juce::Component
{
public:
    LevelSelector (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                   const juce::StringArray& labels, const juce::String& tooltip);

    void resized() override;

    int getSelectedIndex() const noexcept { return selected; }

private:
    void select (int index);

    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelSelector)
};
