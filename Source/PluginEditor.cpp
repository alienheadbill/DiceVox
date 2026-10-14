#include "PluginEditor.h"
#include "Params.h"

namespace Colours
{
    const juce::Colour background { 0xff15141a };
    const juce::Colour panel      { 0xff201f27 };
    const juce::Colour accent     { 0xffe8ff5a };
    const juce::Colour text       { 0xffe6e4ee };
    const juce::Colour dim        { 0xff8a8796 };
}

DiceVoxEditor::DiceVoxEditor (DiceVoxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setUpKnob (inputKnob,  ParamIDs::inputGain,  "INPUT");
    setUpKnob (mixKnob,    ParamIDs::mix,        "MIX");
    setUpKnob (outputKnob, ParamIDs::outputGain, "OUTPUT");

    diceButton.setEnabled (false);
    diceButton.setTooltip ("Randomizer arrives in Phase 3");
    diceButton.setColour (juce::TextButton::buttonColourId, Colours::panel);
    diceButton.setColour (juce::TextButton::textColourOffId, Colours::dim);
    addAndMakeVisible (diceButton);

    bypassButton.setColour (juce::ToggleButton::textColourId, Colours::dim);
    bypassButton.setColour (juce::ToggleButton::tickColourId, Colours::accent);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.state, ParamIDs::bypass, bypassButton);

    setSize (520, 360);
}

void DiceVoxEditor::setUpKnob (LabelledKnob& knob, const juce::String& paramId, const juce::String& name)
{
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId, Colours::accent);
    knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, Colours::panel);
    knob.slider.setColour (juce::Slider::thumbColourId, Colours::text);
    knob.slider.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    addAndMakeVisible (knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.state, paramId, knob.slider);
}

void DiceVoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("DICEVOX", 20, 14, 200, 30, juce::Justification::centredLeft);

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v0.1  |  phase 1 passthrough", getWidth() - 240, 14, 220, 30,
                juce::Justification::centredRight);
}

void DiceVoxEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (40); // title row

    auto bottom = area.removeFromBottom (24);
    bypassButton.setBounds (bottom.removeFromLeft (100));

    diceButton.setBounds (area.removeFromTop (110).withSizeKeepingCentre (160, 90));
    area.removeFromTop (10);

    const int knobWidth = area.getWidth() / 3;
    for (auto* knob : { &inputKnob, &mixKnob, &outputKnob })
    {
        auto column = area.removeFromLeft (knobWidth);
        knob->label.setBounds (column.removeFromTop (18));
        knob->slider.setBounds (column.reduced (8, 0));
    }
}
