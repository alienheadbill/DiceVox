#include "RollChamber.h"
#include "PluginProcessor.h"
#include "Params.h"
#include "dice/DiceRoller.h"
#include "ahe/AHEBrandMarks.h"
#include "ahe/AHELookAndFeel.h"

using namespace ahe::theme;

RollChamber::RollChamber (DiceVoxProcessor& p)
    : processor (p), ufo (ahe::brand::ufoIcon()),
      levelSelector (p.state, ParamIDs::diceLevel, juce::StringArray { "LOW", "MED", "HIGH" },
                     "How wild ROLL goes. LOW: 1-2 subtle effects. MED: 2-4 effects. HIGH: 3-5 effects pushed hard.")
{
    rollButton.getProperties().set (roleProperty, role::primary);
    rollButton.setTooltip ("ROLL: build a new random vocal chain in the chosen style. "
                           "Locked effects stay as they are; Input, Mix and Output are never changed.");
    rollButton.setTitle ("Roll");
    rollButton.setWantsKeyboardFocus (true);
    rollButton.onClick = [this]
    {
        processor.rollDice();
        refresh();

        // Tractor beam: the UFO beams the new sound down (a short animation).
        beamStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    };
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
                         "Glitch: stutter chops, reverse swells, pitch jumps.\n"
                         "Area 51: mothership choirs, crop-circle radio, android abductions.");
    styleBox.setTitle ("Dice style");
    styleBox.setComponentID (ParamIDs::diceStyle);
    addAndMakeVisible (styleBox);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.state, ParamIDs::diceStyle, styleBox);

    addAndMakeVisible (levelSelector);

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

    if (beamStart >= 0.0)
    {
        repaint (iconArea.expanded (8, 6).withBottom (getHeight()));
        if (juce::Time::getMillisecondCounterHiRes() - beamStart > beamMs)
        {
            beamStart = -1.0;
            startTimerHz (8); // back to the slow refresh
            repaint();
        }
    }
}

void RollChamber::resized()
{
    auto area = getLocalBounds().reduced (space::l, space::m);

    rollButton.setBounds (area.removeFromRight (140));
    area.removeFromRight (space::m);
    backButton.setBounds (area.removeFromRight (66).withSizeKeepingCentre (66, 32));
    area.removeFromRight (space::m);
    {
        // STYLE label, style menu, and the LOW / MED / HIGH row under it.
        auto column = area.removeFromRight (160);
        styleLabelArea = column.removeFromTop (14);
        column.removeFromTop (2);
        styleBox.setBounds (column.removeFromTop (26));
        column.removeFromTop (space::xs);
        levelSelector.setBounds (column.removeFromTop (22));
    }
    area.removeFromRight (space::l);

    iconArea = area.removeFromLeft (area.getHeight());
    area.removeFromLeft (space::m);
    textArea = area;
}

float RollChamber::beamAmount() const
{
    if (beamStart < 0.0)
        return 0.0f;
    const auto t = (juce::Time::getMillisecondCounterHiRes() - beamStart) / beamMs;
    return t >= 1.0 ? 0.0f : (float) std::sin (juce::MathConstants<double>::pi * t); // up, then fade out
}

void RollChamber::paint (juce::Graphics& g)
{
    ahe::LookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), radius::hero);

    // Tractor beam under the UFO after a ROLL (purple light: green is kept for
    // "processing is on"), with the UFO bobbing as it beams.
    const float beam = beamAmount();
    auto ufoArea = iconArea.toFloat().translated (0.0f, -3.0f * beam);
    if (beam > 0.0f)
    {
        const float topY = ufoArea.getCentreY() + ufoArea.getHeight() * 0.12f;
        const float bottomY = (float) getHeight() - 4.0f;
        const float cx = ufoArea.getCentreX();
        const float topHalf = ufoArea.getWidth() * 0.12f, bottomHalf = ufoArea.getWidth() * 0.5f;

        juce::Path cone;
        cone.startNewSubPath (cx - topHalf, topY);
        cone.lineTo (cx + topHalf, topY);
        cone.lineTo (cx + bottomHalf, bottomY);
        cone.lineTo (cx - bottomHalf, bottomY);
        cone.closeSubPath();

        juce::ColourGradient light (colour::purpleSoft.withAlpha (0.55f * beam), cx, topY,
                                    colour::purpleSoft.withAlpha (0.0f), cx, bottomY, false);
        g.setGradientFill (light);
        g.fillPath (cone);

        // A few sparks drifting up the beam.
        juce::Random sparks (7);
        const float rise = 1.0f - beam;
        for (int i = 0; i < 6; ++i)
        {
            const float y = bottomY - (bottomY - topY) * std::fmod (sparks.nextFloat() + rise, 1.0f);
            const float span = topHalf + (bottomHalf - topHalf) * (y - topY) / (bottomY - topY);
            const float x = cx + (sparks.nextFloat() * 2.0f - 1.0f) * span * 0.8f;
            g.setColour (juce::Colours::white.withAlpha (0.8f * beam));
            g.fillEllipse (x - 1.2f, y - 1.2f, 2.4f, 2.4f);
        }
    }

    if (ufo != nullptr)
        ufo->drawWithin (g, ufoArea, juce::RectanglePlacement::centred, 1.0f);

    g.setColour (colour::textSecondary);
    g.setFont (juce::Font (font::label (10.0f)));
    g.drawText ("STYLE  +  HOW WILD", styleLabelArea, juce::Justification::bottomLeft, false);

    auto text = textArea;
    auto titleArea = text.removeFromTop (text.getHeight() * 3 / 5);

    g.setColour (colour::textPrimary);
    g.setFont (juce::Font (font::display (26.0f)));
    g.drawFittedText (title, titleArea, juce::Justification::bottomLeft, 1, 0.7f);

    g.setColour (colour::textSecondary);
    g.setFont (juce::Font (font::body (12.0f)));
    g.drawFittedText (summary, text.withTrimmedTop (2), juce::Justification::topLeft, 2, 0.85f);
}
