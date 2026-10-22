#include "PluginEditor.h"
#include "Params.h"
#include "modules/FilterModule.h"
#include "modules/PitchModule.h"
#include "modules/DriveModule.h"
#include "modules/LoFiModule.h"
#include "modules/ChorusModule.h"
#include "modules/ChopModule.h"
#include "modules/ReverseModule.h"
#include "modules/DelayModule.h"
#include "modules/ReverbModule.h"
#include "modules/AlienModule.h"

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

    // 860x690 by default; resizable from 80% (small screens) to 130% with a
    // fixed aspect ratio. The size is not saved with the project.
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (designWidth * minScale), juce::roundToInt (designHeight * minScale),
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
    const float scale = juce::jlimit ((float) minScale, (float) maxScale, (float) getWidth() / (float) designWidth);
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
}

// ============================================================================
// Content (the 860x690 design canvas)
// ============================================================================
DiceVoxEditor::Content::Content (DiceVoxProcessor& p)
    : presetBar (p),
      roll (p),
      inputKnob  (p.state, ParamIDs::inputGain,  "Input",
                  "Input: level going into DiceVox (-24 to +24 dB). Double-click to reset to 0 dB."),
      mixKnob    (p.state, ParamIDs::mix,        "Mix",
                  "Mix: blend between your original vocal (0%) and the processed sound (100%). Double-click to reset."),
      outputKnob (p.state, ParamIDs::outputGain, "Output",
                  "Output: final level leaving DiceVox (-24 to +24 dB). Double-click to reset to 0 dB.")
{
    addAndMakeVisible (header);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (roll);

    // Starfield: fixed seed, so it's the same sky every time the window opens.
    juce::Random sky (51);
    for (int i = 0; i < 140; ++i)
        stars.push_back ({ sky.nextFloat() * (float) designWidth, sky.nextFloat() * (float) designHeight,
                           0.8f + 1.6f * sky.nextFloat() * sky.nextFloat(), 0.08f + 0.45f * sky.nextFloat() * sky.nextFloat() });

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

    addCard ("pitch", "Pitch", "shifts the vocal up or down, as a pitched double or the whole voice.", {
        { PitchModule::shiftId, "Shift", "Shift: semitones up or down (12 = one octave). Up sounds small and bright, down big and dark." },
        { PitchModule::mixId,   "Mix",   "Mix: low = a pitched double under your vocal, 100% = only the pitched voice." } });

    addCard ("drive", "Drive", "saturation for warmth and grit.", {
        { DriveModule::amountId, "Amount", "Amount: how hard the vocal is driven (warm at low settings, gritty at high)." },
        { DriveModule::toneId,   "Tone",   "Tone: lower it to soften the fizz the drive adds." } });

    addCard ("lofi", "Lo-Fi", "bitcrushed, phone-recorded, worn-tape character.", {
        { LoFiModule::crushId,  "Crush",  "Crush: fewer bits and a lower sample rate (dusty, crunchy, darker)." },
        { LoFiModule::wobbleId, "Wobble", "Wobble: tape wow and flutter, a slowly swaying pitch." },
        { LoFiModule::mixId,    "Mix",    "Mix: blend the crushed sound with the clean vocal." } });

    addCard ("alien", "Alien Voice", "your vocal beamed from a spaceship: saucer ring-mod, android robot or space-radio transmission.", {
        { AlienModule::modeId,   "Mode",   "Mode: Saucer (ring-mod wobble to metallic bells), Android (robot voice on one metallic note) or Transmission (crackly space radio)." },
        { AlienModule::amountId, "Amount", "Amount: Saucer = wobble speed / pitch, Android = robot note, Transmission = how rough the signal is." },
        { AlienModule::mixId,    "Mix",    "Mix: blend the alien voice with your clean vocal." } });

    addCard ("chorus", "Chorus", "doubles the vocal and spreads it wide.", {
        { ChorusModule::rateId,  "Rate",  "Rate: how fast the doubled voices drift." },
        { ChorusModule::depthId, "Depth", "Depth: how far the doubled voices drift apart (wider and more wobbly)." } });

    addCard ("chop", "Chop", "tempo-synced stutter: cuts the vocal on and off in rhythm.", {
        { ChopModule::rateId,  "Rate",  "Rate: chop speed in notes, locked to your song's beat (T = triplet)." },
        { ChopModule::depthId, "Depth", "Depth: how far the gaps drop (100% = silent gaps)." } });

    addCard ("reverse", "Reverse", "reversed slices of the vocal that swell in around it.", {
        { ReverseModule::lengthId, "Length", "Length: how long each reversed slice is, in notes at your project tempo." },
        { ReverseModule::mixId,    "Mix",    "Mix: how loud the reversed slices are. Your vocal itself stays at full level." } });

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
    {
        // Preset bar in the header's free centre (between the name and the maker mark).
        const auto centre = header.getCentreArea().translated (header.getX(), header.getY());
        presetBar.setBounds (centre.withSizeKeepingCentre (juce::jmin (400, centre.getWidth()), 30));
    }

    area.reduce (space::l, 0);
    area.removeFromTop (space::m);
    area.removeFromBottom (space::s);

    // Bottom row: bypass on the left, Input / Mix / Output in the middle
    bottomArea = area.removeFromBottom (96);
    {
        auto row = bottomArea.withTrimmedTop (space::s);
        bypassSwitch.setBounds (row.removeFromLeft (140).withSizeKeepingCentre (140, 28));

        constexpr int knobW = 100;
        auto knobRow = row.withSizeKeepingCentre (3 * knobW, row.getHeight());
        for (auto* knob : { &inputKnob, &mixKnob, &outputKnob })
            knob->setBounds (knobRow.removeFromLeft (knobW));
    }

    // ROLL chamber
    roll.setBounds (area.removeFromTop (96));
    area.removeFromTop (space::m);
    area.removeFromBottom (space::m);

    // Effect cards, in signal order: two rows of up to five equal cards
    // (a shorter second row is centred).
    constexpr int perRow = 5;
    const int gap = space::s;
    const int cardW = (area.getWidth() - gap * (perRow - 1)) / perRow;
    const int numRows = ((int) cards.size() + perRow - 1) / perRow;
    const int cardH = (area.getHeight() - gap * (numRows - 1)) / juce::jmax (1, numRows);

    for (int r = 0; r < numRows; ++r)
    {
        auto row = area.removeFromTop (cardH);
        area.removeFromTop (gap);

        const int first = r * perRow;
        const int inRow = juce::jmin (perRow, (int) cards.size() - first);
        row = row.withSizeKeepingCentre (inRow * cardW + (inRow - 1) * gap, cardH);

        for (int c = 0; c < inRow; ++c)
        {
            cards[(size_t) (first + c)]->setBounds (row.removeFromLeft (cardW));
            row.removeFromLeft (gap);
        }
    }
}

void DiceVoxEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (colour::canvas);

    // Deep space: a faint purple nebula glow and a fixed starfield (the same
    // stars every time, so nothing flickers between repaints).
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient nebula (colour::brandPurple.withAlpha (0.16f), bounds.getWidth() * 0.72f, bounds.getHeight() * 0.38f,
                                     colour::canvas.withAlpha (0.0f), bounds.getWidth() * 0.72f + 420.0f, bounds.getHeight() * 0.38f, true);
        g.setGradientFill (nebula);
        g.fillRect (bounds);

        // (Green is reserved for "processing is on", so the second glow is purple too.)
        juce::ColourGradient glow (colour::purpleSoft.withAlpha (0.06f), bounds.getWidth() * 0.12f, bounds.getHeight() * 0.85f,
                                   colour::canvas.withAlpha (0.0f), bounds.getWidth() * 0.12f + 300.0f, bounds.getHeight() * 0.85f, true);
        g.setGradientFill (glow);
        g.fillRect (bounds);

        for (const auto& s : stars)
        {
            g.setColour (juce::Colours::white.withAlpha (s.alpha));
            g.fillEllipse (s.x - s.size * 0.5f, s.y - s.size * 0.5f, s.size, s.size);
        }
    }

    // Divider above the bottom row + build info
    g.setColour (colour::border);
    g.fillRect (bottomArea.getX(), bottomArea.getY(), bottomArea.getWidth(), 1);

    g.setColour (colour::textMuted);
    g.setFont (juce::Font (font::body (11.0f)));
    const auto dot = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    g.drawText ("v" JucePlugin_VersionString + dot + "alien vocal lab" + dot + "test build",
                bottomArea.withTrimmedTop (space::s), juce::Justification::centredRight, false);
}
