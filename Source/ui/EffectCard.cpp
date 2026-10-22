#include "EffectCard.h"
#include "ahe/AHELookAndFeel.h"
#include "modules/FxChain.h"

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
    power.setBounds (top.removeFromRight (58));
    titleArea = top;
    area.removeFromTop (space::s);

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

    ahe::LookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), radius::card, colour::panel,
                                 on ? colour::purpleSoft.withAlpha (0.65f) : colour::border);

    g.setColour (on ? colour::textPrimary : colour::textSecondary);
    g.setFont (juce::Font (font::label (12.0f)));
    g.drawText (title.toUpperCase(), titleArea, juce::Justification::centredLeft, true);
}
