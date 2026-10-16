#include "RollChamber.h"
#include "ahe/AHEBrandMarks.h"
#include "ahe/AHELookAndFeel.h"

using namespace ahe::theme;

RollChamber::RollChamber()
    : ufo (ahe::brand::ufoIcon())
{
    // While ROLL is unavailable the UFO is drawn in a single muted tone: the
    // brand green means "processing is on", so it must not appear on a control
    // that does nothing yet. Phase 3 restores the full-colour mark.
    if (ufo != nullptr)
        ahe::brand::makeMonochrome (*ufo, colour::textMuted);

    setTooltip (juce::String ("ROLL is not available yet. ") + unavailableText
                + ": one click will build a new vocal effect chain.");
    setTitle ("Roll (unavailable)");
    setDescription (unavailableText);
}

void RollChamber::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);

    // Chamber surface with a dashed outline: reads as "slot reserved", not "button".
    g.setColour (colour::panel);
    g.fillRoundedRectangle (area, radius::hero);
    {
        juce::Path outline, dashed;
        outline.addRoundedRectangle (area.reduced (0.5f), radius::hero);
        const float dashes[] = { 6.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (colour::border.brighter (0.15f));
        g.fillPath (dashed);
    }

    // Centre the icon + text group in the chamber.
    const juce::Font titleFont (font::display (34.0f));
    const juce::Font pillFont (font::label (9.5f));
    const juce::Font infoFont (font::body (12.5f));
    const juce::String pillText ("NOT AVAILABLE YET");

    const float iconSize = area.getHeight() - 2.0f * (float) space::l;
    const float titleW   = (float) juce::GlyphArrangement::getStringWidthInt (titleFont, "ROLL");
    const float pillW    = (float) juce::GlyphArrangement::getStringWidthInt (pillFont, pillText) + 16.0f;
    const float infoW    = (float) juce::GlyphArrangement::getStringWidthInt (infoFont, unavailableText);
    const float textW    = juce::jmax (titleW + (float) space::l + pillW, infoW);
    const float groupW   = iconSize + (float) space::xl + textW;

    auto group = area.withSizeKeepingCentre (groupW, iconSize);

    // UFO mark (secondary motif, decorative only)
    auto iconArea = group.removeFromLeft (iconSize);
    if (ufo != nullptr)
        ufo->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 0.8f);
    group.removeFromLeft ((float) space::xl);

    auto title = group.removeFromTop (group.getHeight() * 0.56f);
    auto info  = group.withTrimmedTop (2.0f);

    g.setColour (colour::textMuted);
    g.setFont (titleFont);
    g.drawText ("ROLL", title.withWidth (titleW + 4.0f).toNearestInt(), juce::Justification::bottomLeft, false);

    // Status pill next to the title: shape + text, never colour alone
    auto pill = juce::Rectangle<float> (pillW, 17.0f)
                    .withPosition (title.getX() + titleW + (float) space::l, title.getBottom() - 24.0f);
    g.setColour (colour::textMuted);
    g.drawRoundedRectangle (pill.reduced (0.5f), 8.5f, 1.0f);
    g.setColour (colour::textSecondary);
    g.setFont (pillFont);
    g.drawText (pillText, pill.toNearestInt(), juce::Justification::centred, false);

    g.setColour (colour::textSecondary);
    g.setFont (infoFont);
    g.drawText (unavailableText, info.toNearestInt(), juce::Justification::topLeft, false);
}
