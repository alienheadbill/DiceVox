#include "AHEKnob.h"

namespace ahe
{
    using namespace theme;

    Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                const juce::String& displayName, const juce::String& tooltip)
        : paramId (id)
    {
        auto* param = state.getParameter (paramId);
        jassert (param != nullptr); // unknown parameter ID

        nameLabel.setText (displayName.toUpperCase(), juce::dontSendNotification);
        nameLabel.setFont (juce::Font (font::label (11.0f)));
        nameLabel.setColour (juce::Label::textColourId, colour::textSecondary);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (nameLabel);

        slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        slider.setTextBoxIsEditable (true);
        slider.setWantsKeyboardFocus (true);
        slider.setTooltip (tooltip);
        slider.setTitle (displayName);
        slider.setDescription (tooltip);
        slider.setComponentID (paramId);   // lets tests and tools find the control by parameter ID
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

        if (param != nullptr)
        {
            // Unit suffix from the parameter itself, e.g. " dB" or " %".
            if (const auto unit = param->getLabel(); unit.isNotEmpty())
                slider.setTextValueSuffix (" " + unit);

            // Double-click the knob to return to the parameter's default.
            slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }
    }

    void Knob::setCompact (bool shouldBeCompact)
    {
        slider.getProperties().set ("ahe.compact", shouldBeCompact);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, shouldBeCompact ? 16 : 20);
        nameLabel.setFont (juce::Font (font::label (shouldBeCompact ? 10.0f : 11.0f)));
        resized();
    }

    void Knob::resized()
    {
        auto area = getLocalBounds();
        nameLabel.setBounds (area.removeFromTop (16));
        area.removeFromTop (space::xs);
        slider.setBounds (area);
    }
}
