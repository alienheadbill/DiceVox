#include "EffectCard.h"
#include "ahe/AHELookAndFeel.h"
#include "modules/FxChain.h"
#include "Params.h"

using namespace ahe::theme;

EffectCard::EffectCard (juce::AudioProcessorValueTreeState& state, const juce::String& moduleId,
                        const juce::String& cardTitle, const juce::String& description,
                        const std::vector<KnobSpec>& knobSpecs)
    : title (cardTitle)
{
    const auto powerId = FxChain::enabledParamId (moduleId);

    power.getProperties().set (roleProperty, role::bypass);
    power.getProperties().set ("ahe.onText", "ON");
    power.getProperties().set ("ahe.offText", "OFF");
    power.getProperties().set ("ahe.onMeansActive", true);
    power.setTooltip (cardTitle + ": " + description + " Click to switch it on or off.");
    power.setTitle (cardTitle + " on/off");
    power.setWantsKeyboardFocus (true);
    power.setComponentID (powerId); // lets tests and tools find the control by parameter ID
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, powerId, power);

    // Repaint the card outline when the effect is switched (automation and dice included).
    power.onStateChange = [this] { repaint(); };

    // Dice lock: ROLL leaves a locked effect exactly as it is.
    const auto lockId = ParamIDs::diceLock (moduleId);
    lock.getProperties().set (roleProperty, role::lock);
    lock.setClickingTogglesState (true);
    lock.setTooltip ("Lock " + cardTitle + ": ROLL keeps it exactly as it is (on or off, same settings) "
                     "and only rerolls the other effects.");
    lock.setTitle ("Lock " + cardTitle);
    lock.setWantsKeyboardFocus (true);
    lock.setComponentID (lockId);
    addAndMakeVisible (lock);
    lockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, lockId, lock);

    for (auto& spec : knobSpecs)
    {
        auto knob = std::make_unique<ahe::Knob> (state, spec.paramId, spec.name, spec.tooltip);
        knob->setCompact (true);
        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    setTitle (cardTitle);
    setDescription (description);
}

void EffectCard::resized()
{
    auto area = getLocalBounds().reduced (space::s + 2, space::s);

    auto top = area.removeFromTop (24);
    power.setBounds (top.removeFromRight (62)); // wide enough for "OFF" next to its indicator
    top.removeFromRight (space::xs);
    lock.setBounds (top.removeFromRight (24).withSizeKeepingCentre (24, 22));
    top.removeFromRight (space::xs);
    titleArea = top;
    area.removeFromTop (space::xs);

    // Two knobs per row; a lone last knob is centred.
    const int numKnobs = (int) knobs.size();
    const int numRows  = (numKnobs + 1) / 2;
    if (numRows == 0)
        return;

    const int rowH = juce::jmin (100, area.getHeight() / numRows);
    for (int i = 0; i < numKnobs; i += 2)
    {
        auto row = area.removeFromTop (rowH);
        const int colW = row.getWidth() / 2;

        if (i + 1 < numKnobs)
        {
            knobs[(size_t) i]->setBounds (row.removeFromLeft (colW));
            knobs[(size_t) i + 1]->setBounds (row);
        }
        else
        {
            knobs[(size_t) i]->setBounds (row.withSizeKeepingCentre (colW, rowH));
        }
    }
}

void EffectCard::paint (juce::Graphics& g)
{
    const bool on = power.getToggleState();
    const auto area = getLocalBounds().toFloat().reduced (0.5f);

    // A sci-fi module: slightly see-through (the starfield shows faintly), and
    // when the effect is processing it powers up with a green rim and a green
    // "power rail" along the top (green = processing is on).
    ahe::LookAndFeel::drawPanel (g, area, radius::card, colour::panel.withAlpha (0.88f),
                                 on ? colour::alienGreen.withAlpha (0.7f) : colour::border);

    if (on)
    {
        juce::ColourGradient wash (colour::alienGreen.withAlpha (0.10f), area.getCentreX(), area.getY(),
                                   colour::alienGreen.withAlpha (0.0f), area.getCentreX(), area.getY() + 60.0f, false);
        g.setGradientFill (wash);
        g.fillRoundedRectangle (area.reduced (1.0f), radius::card);

        g.setColour (colour::alienGreen);
        g.fillRoundedRectangle (area.getX() + radius::card, area.getY() + 1.5f, area.getWidth() - 2.0f * radius::card, 2.0f, 1.0f);
    }

    g.setColour (on ? colour::textPrimary : colour::textSecondary);
    g.setFont (juce::Font (font::label (11.5f)));
    g.drawFittedText (title.toUpperCase(), titleArea, juce::Justification::centredLeft, 1, 0.75f);
}
