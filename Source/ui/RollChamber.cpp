#include "RollChamber.h"
#include "PluginProcessor.h"
#include "Params.h"
#include "dice/DiceRoller.h"
#include "ahe/AHEBrandMarks.h"
#include "ahe/AHELookAndFeel.h"

using namespace ahe::theme;

RollChamber::RollChamber (DiceVoxProcessor& p)
    : processor (p), ufo (ahe::brand::ufoIcon()),
      intensityKnob (p.state, ParamIDs::diceIntensity, "Intensity",
                     "Intensity: how far ROLL goes. Low = 1-3 subtle effects, high = 3-5 effects pushed hard.")
{
    rollButton.getProperties().set (roleProperty, role::primary);
    rollButton.setTooltip ("ROLL: build a new random vocal chain in the chosen style. "
                           "Locked effects stay as they are; Input, Mix and Output are never changed.");
    rollButton.setTitle ("Roll");
    rollButton.setWantsKeyboardFocus (true);
    rollButton.onClick = [this] { processor.rollDice(); refresh(); };
    addAndMakeVisible (rollButton);

    backButton.setTooltip ("BACK: return to the effects you had before the last roll.");
    backButton.setTitle ("Back to the previous roll");
    backButton.setWantsKeyboardFocus (true);
    backButton.onClick = [this] { processor.undoRoll(); refresh(); };
    addAndMakeVisible (backButton);

    // Style: the item IDs are 1-based, in the parameter's choice order.
    styleBox.addItemList (DiceRoller::styleNames(), 1);
    styleBox.setTooltip ("STYLE steers what ROLL picks.\n"
                         "Any Vibe: a bit of everything.\n"
                         "Dreamy Pop: airy doubles, slap and dotted echoes, big glossy reverb.\n"
                         "Dark Toy Box: lo-fi crunch, telephone filters, eerie pitch doubles.\n"
                         "Pitched Rage: pitched-up or -down vocals, grit, echo throws, chops.\n"
                         "Lo-Fi Bedroom: tape wobble, crushed and muffled, roomy.\n"
                         "Glitch: stutter chops, reverse swells, pitch jumps.");
    styleBox.setTitle ("Dice style");
    styleBox.setComponentID (ParamIDs::diceStyle);
    addAndMakeVisible (styleBox);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.state, ParamIDs::diceStyle, styleBox);

    intensityKnob.setCompact (true);
    addAndMakeVisible (intensityKnob);

    refresh();
    startTimerHz (8);
}

void RollChamber::refresh()
{
    // Active effects, in chain order.
    juce::StringArray active;
    for (int i = 0; i < processor.chain.getNumModules(); ++i)
    {
        auto& module = processor.chain.getModule (i);
        if (auto* on = processor.state.getRawParameterValue (FxChain::enabledParamId (module.getId())))
            if (on->load() > 0.5f)
                active.add (module.getDisplayName());
    }

    const auto rollName = processor.getRollName();
    auto newTitle = rollName.isNotEmpty() ? rollName.toUpperCase()
                                          : juce::String (active.isEmpty() ? emptyTitle : "CUSTOM CHAIN");
    auto newSummary = active.isEmpty() ? juce::String (emptySummary)
                                       : active.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")));

    if (newTitle != title || newSummary != summary)
    {
        title = newTitle;
        summary = newSummary;
        setTitle ("Roll: " + title);
        setDescription (summary);
        repaint();
    }

    backButton.setEnabled (processor.canUndoRoll());
}

void RollChamber::timerCallback()
{
    refresh();
}

void RollChamber::resized()
{
    auto area = getLocalBounds().reduced (space::l, space::m);

    rollButton.setBounds (area.removeFromRight (140));
    area.removeFromRight (space::m);
    backButton.setBounds (area.removeFromRight (66).withSizeKeepingCentre (66, 32));
    area.removeFromRight (space::m);
    intensityKnob.setBounds (area.removeFromRight (72));
    area.removeFromRight (space::s);
    {
        auto styleArea = area.removeFromRight (150);
        styleLabelArea = styleArea.removeFromTop (styleArea.getHeight() / 2 - 2).withTrimmedTop (4);
        styleBox.setBounds (styleArea.removeFromTop (28));
    }
    area.removeFromRight (space::l);

    iconArea = area.removeFromLeft (area.getHeight());
    area.removeFromLeft (space::m);
    textArea = area;
}

void RollChamber::paint (juce::Graphics& g)
{
    ahe::LookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), radius::hero);

    if (ufo != nullptr)
        ufo->drawWithin (g, iconArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);

    g.setColour (colour::textSecondary);
    g.setFont (juce::Font (font::label (10.0f)));
    g.drawText ("STYLE", styleLabelArea, juce::Justification::bottomLeft, false);

    auto text = textArea;
    auto titleArea = text.removeFromTop (text.getHeight() * 3 / 5);

    g.setColour (colour::textPrimary);
    g.setFont (juce::Font (font::display (26.0f)));
    g.drawFittedText (title, titleArea, juce::Justification::bottomLeft, 1, 0.7f);

    g.setColour (colour::textSecondary);
    g.setFont (juce::Font (font::body (12.0f)));
    g.drawFittedText (summary, text.withTrimmedTop (2), juce::Justification::topLeft, 2, 0.85f);
}
