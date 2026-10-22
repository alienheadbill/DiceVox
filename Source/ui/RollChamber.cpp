#include "RollChamber.h"
#include "PluginProcessor.h"
#include "ahe/AHEBrandMarks.h"
#include "ahe/AHELookAndFeel.h"

using namespace ahe::theme;

RollChamber::RollChamber (DiceVoxProcessor& p)
    : processor (p), ufo (ahe::brand::ufoIcon())
{
    rollButton.getProperties().set (roleProperty, role::primary);
    rollButton.setTooltip ("ROLL: build a new random vocal chain (2 to 4 effects with settings that suit vocals). "
                           "Input, Mix and Output stay as they are.");
    rollButton.setTitle ("Roll");
    rollButton.setWantsKeyboardFocus (true);
    rollButton.onClick = [this] { processor.rollDice(); refresh(); };
    addAndMakeVisible (rollButton);

    backButton.setTooltip ("BACK: return to the effects you had before the last roll.");
    backButton.setTitle ("Back to the previous roll");
    backButton.setWantsKeyboardFocus (true);
    backButton.onClick = [this] { processor.undoRoll(); refresh(); };
    addAndMakeVisible (backButton);

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

    rollButton.setBounds (area.removeFromRight (150));
    area.removeFromRight (space::m);
    backButton.setBounds (area.removeFromRight (76).withSizeKeepingCentre (76, 34));
    area.removeFromRight (space::l);

    iconArea = area.removeFromLeft (area.getHeight());
    area.removeFromLeft (space::l);
    textArea = area;
}

void RollChamber::paint (juce::Graphics& g)
{
    ahe::LookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), radius::hero);

    if (ufo != nullptr)
        ufo->drawWithin (g, iconArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);

    auto text = textArea;
    auto titleArea = text.removeFromTop (text.getHeight() * 3 / 5);

    g.setColour (colour::textPrimary);
    g.setFont (juce::Font (font::display (30.0f)));
    g.drawFittedText (title, titleArea, juce::Justification::bottomLeft, 1, 0.8f);

    g.setColour (colour::textSecondary);
    g.setFont (juce::Font (font::body (12.5f)));
    g.drawFittedText (summary, text.withTrimmedTop (2), juce::Justification::topLeft, 1, 0.9f);
}
