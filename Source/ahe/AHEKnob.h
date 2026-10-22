#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AHETheme.h"

namespace ahe
{
    // -----------------------------------------------------------------------
    // Knob — labelled rotary control bound to one APVTS parameter.
    //
    //   INPUT          <- plain-language name
    //   ( knob )       <- drag / scroll / arrow keys; double-click resets to default
    //   0.0 dB         <- value with the parameter's unit; double-click to type a value
    //
    // The attachment is the standard SliderAttachment, so automation, undo and
    // host recall behave exactly as before. Units come from the parameter's own
    // label ("dB", "%"), so they always match what the host shows.
    // -----------------------------------------------------------------------
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
              const juce::String& displayName, const juce::String& tooltip);

        void resized() override;

        // Smaller name and value text, for knobs inside effect cards. Call it
        // before the editor applies its look-and-feel (i.e. at construction).
        void setCompact (bool shouldBeCompact);

        juce::Slider& getSlider() noexcept { return slider; }
        const juce::String& getParameterId() const noexcept { return paramId; }

    private:
        juce::String paramId;
        juce::Label  nameLabel;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };
}
