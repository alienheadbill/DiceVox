#include "SignalScope.h"
#include "PluginProcessor.h"

using namespace ahe::theme;

SignalScope::SignalScope (DiceVoxProcessor& p)
    : processor (p), points ((size_t) DiceVoxProcessor::scopeSize, 0.0f)
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Signal scope");
    setDescription ("Live waveform of the vocal leaving DiceVox.");
    startTimerHz (30);
}

void SignalScope::timerCallback()
{
    refreshNow();
}

void SignalScope::refreshNow()
{
    processor.copyScope (points.data());
    repaint();
}

void SignalScope::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Label
    auto labelArea = area.removeFromTop (14.0f);
    g.setColour (colour::textMuted);
    g.setFont (juce::Font (font::label (9.5f)));
    g.drawText ("SIGNAL", labelArea, juce::Justification::centredLeft, false);

    // Screen: a dark inset with a hairline rim and faint graticule.
    auto screen = area.reduced (0.0f, 2.0f);
    g.setColour (colour::canvas.withAlpha (0.7f));
    g.fillRoundedRectangle (screen, 6.0f);
    g.setColour (colour::border);
    g.drawRoundedRectangle (screen.reduced (0.5f), 6.0f, 1.0f);

    auto plot = screen.reduced (6.0f, 5.0f);
    const float mid = plot.getCentreY();
    const float half = plot.getHeight() * 0.5f;

    g.setColour (colour::purpleSoft.withAlpha (0.06f));
    for (int i = 1; i < 8; ++i)
        g.fillRect (plot.getX() + plot.getWidth() * (float) i / 8.0f, plot.getY(), 1.0f, plot.getHeight());
    g.setColour (colour::purpleSoft.withAlpha (0.18f));
    g.fillRect (plot.getX(), mid, plot.getWidth(), 1.0f);

    // Mirrored peak waveform (square-root scaled so quiet singing still shows).
    const int n = (int) points.size();
    if (n < 2)
        return;

    juce::Path top, fill;
    for (int i = 0; i < n; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
        const float level = std::sqrt (juce::jlimit (0.0f, 1.0f, points[(size_t) i])) * half;
        if (i == 0) { top.startNewSubPath (x, mid - level); fill.startNewSubPath (x, mid - level); }
        else        { top.lineTo (x, mid - level);          fill.lineTo (x, mid - level); }
    }
    for (int i = n - 1; i >= 0; --i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
        const float level = std::sqrt (juce::jlimit (0.0f, 1.0f, points[(size_t) i])) * half;
        fill.lineTo (x, mid + level);
    }
    fill.closeSubPath();

    juce::ColourGradient body (colour::brandPurple.withAlpha (0.45f), 0.0f, plot.getY(),
                               colour::brandPurple.withAlpha (0.45f), 0.0f, plot.getBottom(), false);
    body.addColour (0.5, colour::purpleSoft.withAlpha (0.15f));
    g.setGradientFill (body);
    g.fillPath (fill);

    g.setColour (colour::brandPurple.withAlpha (0.35f));
    g.strokePath (top, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour::purpleSoft);
    g.strokePath (top, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
