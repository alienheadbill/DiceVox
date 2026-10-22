#include "PluginEditor.h"
#include "Params.h"
#include "modules/FilterModule.h"
#include "modules/DriveModule.h"
#include "modules/ChorusModule.h"
#include "modules/DelayModule.h"
#include "modules/ReverbModule.h"

using namespace ahe::theme;

// ============================================================================
// Editor
// ============================================================================
DiceVoxEditor::DiceVoxEditor (DiceVoxProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    // Add children before applying the look-and-feel: setLookAndFeel() makes
    // existing children rebuild their look-and-feel-created parts (e.g. the knob
    // value boxes). Children added afterwards would keep JUCE's default style.
    addAndMakeVisible (content);
    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);

    // 800x530 by default; optional resize up to 150% with a fixed aspect ratio.
    // The size is not saved with the project.
    setResizable (true, true);
    setResizeLimits (designWidth, designHeight,
                     juce::roundToInt (designWidth * maxScale), juce::roundToInt (designHeight * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    setSize (designWidth, designHeight);
}

DiceVoxEditor::~DiceVoxEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DiceVoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour::canvas);
}

void DiceVoxEditor::resized()
{
    const float scale = juce::jlimit (1.0f, (float) maxScale, (float) getWidth() / (float) designWidth);
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
}

// ============================================================================
// Content (the 800x530 design canvas)
// ============================================================================
DiceVoxEditor::Content::Content (DiceVoxProcessor& p)
    : roll (p),
      inputKnob  (p.state, ParamIDs::inputGain,  "Input",
                  "Input: level going into DiceVox (-24 to +24 dB). Double-click to reset to 0 dB."),
      mixKnob    (p.state, ParamIDs::mix,        "Mix",
                  "Mix: blend between your original vocal (0%) and the processed sound (100%). Double-click to reset."),
      outputKnob (p.state, ParamIDs::outputGain, "Output",
                  "Output: final level leaving DiceVox (-24 to +24 dB). Double-click to reset to 0 dB.")
{
    addAndMakeVisible (header);
    addAndMakeVisible (roll);

    // One card per effect, in signal order.
    auto addCard = [this, &p] (const juce::String& moduleId, const juce::String& title,
                               const juce::String& description, std::vector<EffectCard::KnobSpec> knobs)
    {
        cards.push_back (std::make_unique<EffectCard> (p.state, moduleId, title, description, knobs));
        addAndMakeVisible (*cards.back());
    };

    addCard ("filter", "Filter", "cuts lows and highs, from rumble clean-up to telephone and radio sounds.", {
        { FilterModule::lowCutId,    "Low Cut",   "Low Cut: removes everything below this frequency (thinner, smaller voice as you raise it)." },
        { FilterModule::highCutId,   "High Cut",  "High Cut: removes everything above this frequency (darker, muffled voice as you lower it)." },
        { FilterModule::resonanceId, "Resonance", "Resonance: a boost right at the cutoffs, for a nasal, honky edge." } });

    addCard ("drive", "Drive", "saturation for warmth and grit.", {
        { DriveModule::amountId, "Amount", "Amount: how hard the vocal is driven (warm at low settings, gritty at high)." },
        { DriveModule::toneId,   "Tone",   "Tone: lower it to soften the fizz the drive adds." } });

    addCard ("chorus", "Chorus", "doubles the vocal and spreads it wide.", {
        { ChorusModule::rateId,  "Rate",  "Rate: how fast the doubled voices drift." },
        { ChorusModule::depthId, "Depth", "Depth: how far the doubled voices drift apart (wider and more wobbly)." } });

    addCard ("delay", "Delay", "tempo-synced echoes that bounce left and right.", {
        { DelayModule::timeId,     "Time",     "Time: echo length in notes, synced to your project tempo (D = dotted, T = triplet)." },
        { DelayModule::feedbackId, "Feedback", "Feedback: how many times the echo repeats." },
        { DelayModule::mixId,      "Mix",      "Mix: how loud the echoes are. Your vocal itself stays at full level." } });

    addCard ("reverb", "Reverb", "room, space or hall around the vocal.", {
        { ReverbModule::sizeId, "Size", "Size: from a small room to a long hall." },
        { ReverbModule::mixId,  "Mix",  "Mix: how much reverb is added. Your vocal itself stays upfront." } });

    for (auto* knob : { &inputKnob, &mixKnob, &outputKnob })
        addAndMakeVisible (*knob);

    // Bypass: ON = bypassed. The switch shows text + an indicator shape for each state.
    bypassSwitch.getProperties().set (roleProperty, role::bypass);
    bypassSwitch.getProperties().set ("ahe.onText", "BYPASSED");
    bypassSwitch.getProperties().set ("ahe.offText", "PROCESSING");
    bypassSwitch.getProperties().set ("ahe.onMeansActive", false);
    bypassSwitch.setTooltip ("Bypass: switch DiceVox off to compare with your original vocal.");
    bypassSwitch.setTitle ("Bypass");
    bypassSwitch.setWantsKeyboardFocus (true);
    bypassSwitch.setComponentID (ParamIDs::bypass);
    addAndMakeVisible (bypassSwitch);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.state, ParamIDs::bypass, bypassSwitch);

    // Repaint the switch label whenever bypass changes (host automation included).
    bypassSwitch.onStateChange = [this] { bypassSwitch.repaint(); };
}

void DiceVoxEditor::Content::resized()
{
    auto area = getLocalBounds();

    header.setBounds (area.removeFromTop (56));

    area.reduce (space::l, 0);
    area.removeFromTop (space::m);
    area.removeFromBottom (space::s);

    // Bottom row: bypass on the left, Input / Mix / Output in the middle
    bottomArea = area.removeFromBottom (100);
    {
        auto row = bottomArea.withTrimmedTop (space::s);
        bypassSwitch.setBounds (row.removeFromLeft (140).withSizeKeepingCentre (140, 28));

        constexpr int knobW = 100;
        auto knobRow = row.withSizeKeepingCentre (3 * knobW, row.getHeight());
        for (auto* knob : { &inputKnob, &mixKnob, &outputKnob })
            knob->setBounds (knobRow.removeFromLeft (knobW));
    }

    // ROLL chamber
    roll.setBounds (area.removeFromTop (88));
    area.removeFromTop (space::m);
    area.removeFromBottom (space::m);

    // Effect cards: equal columns
    const int gap = space::s;
    const int numCards = (int) cards.size();
    const int cardW = (area.getWidth() - gap * (numCards - 1)) / juce::jmax (1, numCards);
    for (auto& card : cards)
    {
        card->setBounds (area.removeFromLeft (cardW));
        area.removeFromLeft (gap);
    }
}

void DiceVoxEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (colour::canvas);

    // Divider above the bottom row + build info
    g.setColour (colour::border);
    g.fillRect (bottomArea.getX(), bottomArea.getY(), bottomArea.getWidth(), 1);

    g.setColour (colour::textMuted);
    g.setFont (juce::Font (font::body (11.0f)));
    const auto dot = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    g.drawText ("v" JucePlugin_VersionString + dot + "test build",
                bottomArea.withTrimmedTop (space::s), juce::Justification::centredRight, false);
}
