#include "LevelSelector.h"

using namespace ahe::theme;

LevelSelector::LevelSelector (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                              const juce::StringArray& labels, const juce::String& tooltip)
{
    auto* param = state.getParameter (paramId);
    jassert (param != nullptr); // unknown parameter ID

    setComponentID (paramId); // lets tests and tools find the control by parameter ID
    setTitle (param != nullptr ? param->getName (64) : paramId);

    for (int i = 0; i < labels.size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton> (labels[i]);
        button->getProperties().set (roleProperty, role::segment);
        button->setTooltip (tooltip);
        button->setWantsKeyboardFocus (true);
        button->onClick = [this, i]
        {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }

    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (
            *param, [this] (float value) { select (juce::roundToInt (value)); }, nullptr);
        attachment->sendInitialUpdate();
    }
}

void LevelSelector::select (int index)
{
    selected = index;
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState ((int) i == index, juce::dontSendNotification);
}

void LevelSelector::resized()
{
    auto area = getLocalBounds();
    const int count = juce::jmax (1, (int) buttons.size());
    const int gap = 2;
    const int w = (area.getWidth() - gap * (count - 1)) / count;

    for (auto& b : buttons)
    {
        b->setBounds (area.removeFromLeft (w));
        area.removeFromLeft (gap);
    }
}
