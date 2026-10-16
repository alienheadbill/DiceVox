#include "PluginEditor.h"
#include "Params.h"

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

    // Compact 520x360 by default; optional resize up to 150% with a fixed aspect
    // ratio. PROVISIONAL until verified as a VST3 in FL Studio on Windows.
    // The size is not saved with the project (that would change the plug-in
    // state format, which is out of scope for this milestone).
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
// Content (the 520x360 design canvas)
// ============================================================================
DiceVoxEditor::Content::Content (DiceVoxProcessor& p)
    : inputKnob  (p.state, ParamIDs::inputGain,  "Input",
                  "Input: level going into DiceVox (-24 to +24 dB). Double-click to reset to 0 dB."),
      mixKnob    (p.state, ParamIDs::mix,        "Mix",
                  "Mix: blend between your original vocal (0%) and the processed sound (100%). Double-click to reset."),
      outputKnob (p.state, ParamIDs::outputGain, "Output",
                  "Output: final level leaving DiceVox (-24 to +24 dB). Double-click to reset to 0 dB.")
{
    addAndMakeVisible (header);

    // Navigation: QUICK is the only view that exists today.
    for (auto* tab : { &quickTab, &advancedTab })
    {
        tab->getProperties().set (roleProperty, role::tab);
        tab->setClickingTogglesState (false);
        tab->setWantsKeyboardFocus (true);
        addAndMakeVisible (*tab);
    }
    quickTab.setToggleState (true, juce::dontSendNotification);
    quickTab.setTooltip ("Quick view: the main controls.");
    quickTab.setTitle ("Quick view (selected)");

    // Development-only badge: the shipping UI must not show phase labels.
    advancedTab.getProperties().set ("ahe.badge", "PHASE 5");
    advancedTab.setEnabled (false);
    advancedTab.setTooltip ("Advanced view (every effect setting) arrives in Phase 5.");
    advancedTab.setTitle ("Advanced view (not available until Phase 5)");

    addAndMakeVisible (roll);

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

    // Header (56) with tabs in its free centre area
    header.setBounds (area.removeFromTop (56));
    {
        auto tabs = header.getCentreArea().withSizeKeepingCentre (header.getCentreArea().getWidth(), 30)
                                          .translated (0, header.getY());
        quickTab.setBounds (tabs.removeFromLeft (64));
        tabs.removeFromLeft (space::xs);
        advancedTab.setBounds (tabs.removeFromLeft (juce::jmin (148, tabs.getWidth())));
    }

    area.reduce (space::l, 0);
    area.removeFromTop (space::m + 2);

    // Footer: bypass switch on the left, build info on the right
    footerArea = area.removeFromBottom (44);
    bypassSwitch.setBounds (footerArea.withTrimmedTop (space::s).withTrimmedBottom (space::s)
                                      .removeFromLeft (140));

    // ROLL chamber
    roll.setBounds (area.removeFromTop (100));
    area.removeFromTop (space::m);
    area.removeFromBottom (space::s);   // breathing room above the footer line

    // Knob row: three equal columns
    const int columnW = area.getWidth() / 3;
    for (auto* knob : { &inputKnob, &mixKnob, &outputKnob })
        knob->setBounds (area.removeFromLeft (columnW).withSizeKeepingCentre (110, area.getHeight()));
}

void DiceVoxEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (colour::canvas);

    // Footer divider + build/phase info (honest about what this build does)
    g.setColour (colour::border);
    g.fillRect (footerArea.getX(), footerArea.getY(), footerArea.getWidth(), 1);

    g.setColour (colour::textMuted);
    g.setFont (juce::Font (font::body (11.0f)));
    const auto dot = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    g.drawText ("v" JucePlugin_VersionString + dot + "Phase 1 preview" + dot + "no effects yet",
                footerArea.withTrimmedRight (space::l), juce::Justification::centredRight, false);
}
