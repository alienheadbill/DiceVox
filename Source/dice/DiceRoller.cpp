#include "DiceRoller.h"
#include "modules/FxChain.h"
#include "modules/FilterModule.h"
#include "modules/DriveModule.h"
#include "modules/ChorusModule.h"
#include "modules/DelayModule.h"
#include "modules/ReverbModule.h"

namespace
{
    void setValue (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
    {
        if (auto* param = state.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
        else
        {
            jassertfalse; // unknown parameter ID
        }
    }

    float uniform (juce::Random& rng, float lo, float hi)    { return lo + rng.nextFloat() * (hi - lo); }
    float logUniform (juce::Random& rng, float lo, float hi) { return lo * std::pow (hi / lo, rng.nextFloat()); }

    struct FilterStyle { const char* name; float lowCutLo, lowCutHi, highCutLo, highCutHi, resonanceLo, resonanceHi; };
    constexpr FilterStyle filterStyles[] = {
        { "Telephone", 250.0f,  500.0f, 2500.0f,  4000.0f, 10.0f, 50.0f },
        { "Radio",     500.0f,  900.0f, 3000.0f,  5000.0f, 30.0f, 70.0f },
        { "Thin",      400.0f, 1200.0f, 12000.0f, 20000.0f, 0.0f, 30.0f },
        { "Muffled",    40.0f,  100.0f,  800.0f,  2000.0f,  0.0f, 40.0f },
    };

    // Delay divisions (indices into DelayModule::divisionNames), repeated to
    // weight them: 1/8, 1/8 dotted and 1/4 are the classic vocal throws.
    constexpr int delayDivisions[] = { 2, 2, 3, 3, 3, 5, 5, 0, 1, 4, 6, 7 };
}

const juce::StringArray& DiceRoller::moduleIds()
{
    static const juce::StringArray ids { "filter", "drive", "chorus", "delay", "reverb" };
    return ids;
}

juce::String DiceRoller::roll (juce::AudioProcessorValueTreeState& state, juce::Random& rng)
{
    // 1. Which effects: 2-4 of them, weighted.
    struct Weighted { juce::String id; float weight; };
    std::vector<Weighted> pool { { "filter", 0.8f }, { "drive", 0.7f }, { "chorus", 0.7f },
                                 { "delay", 1.0f }, { "reverb", 1.3f } };
    juce::StringArray chosen;

    for (int count = 2 + rng.nextInt (3); count > 0 && ! pool.empty(); --count)
    {
        float total = 0.0f;
        for (auto& w : pool)
            total += w.weight;

        float pick = rng.nextFloat() * total;
        size_t k = 0;
        for (; k + 1 < pool.size(); ++k)
        {
            pick -= pool[k].weight;
            if (pick < 0.0f)
                break;
        }

        chosen.add (pool[k].id);
        pool.erase (pool.begin() + (std::ptrdiff_t) k);
    }

    juce::StringArray adjectives, nouns;

    // 2. Settings for each chosen effect, in chain order (so the name reads well).
    if (chosen.contains ("filter"))
    {
        const auto& s = filterStyles[rng.nextInt ((int) std::size (filterStyles))];
        setValue (state, FilterModule::lowCutId,    logUniform (rng, s.lowCutLo, s.lowCutHi));
        setValue (state, FilterModule::highCutId,   logUniform (rng, s.highCutLo, s.highCutHi));
        setValue (state, FilterModule::resonanceId, uniform (rng, s.resonanceLo, s.resonanceHi));
        adjectives.add (s.name);
    }

    if (chosen.contains ("drive"))
    {
        const float amount = uniform (rng, 20.0f, 80.0f);
        setValue (state, DriveModule::amountId, amount);
        setValue (state, DriveModule::toneId, uniform (rng, 30.0f, 90.0f));
        adjectives.add (amount < 45.0f ? "Warm" : "Gritty");
    }

    if (chosen.contains ("chorus"))
    {
        const float depth = uniform (rng, 25.0f, 80.0f);
        setValue (state, ChorusModule::rateId, logUniform (rng, 0.2f, 2.5f));
        setValue (state, ChorusModule::depthId, depth);
        adjectives.add (depth < 50.0f ? "Doubled" : "Wide");
    }

    if (chosen.contains ("delay"))
    {
        const int division = delayDivisions[rng.nextInt ((int) std::size (delayDivisions))];
        setValue (state, DelayModule::timeId, (float) division);
        setValue (state, DelayModule::feedbackId, uniform (rng, 15.0f, 60.0f));
        setValue (state, DelayModule::mixId, uniform (rng, 15.0f, 45.0f));
        nouns.add (division == 0 ? "Slap" : division == 7 ? "Throw" : "Echo");
    }

    if (chosen.contains ("reverb"))
    {
        const float size = uniform (rng, 30.0f, 95.0f);
        setValue (state, ReverbModule::sizeId, size);
        setValue (state, ReverbModule::mixId, uniform (rng, 15.0f, 50.0f));
        nouns.add (size < 50.0f ? "Room" : size < 78.0f ? "Space" : "Hall");
    }

    // 3. Switch the chosen effects on and the rest off.
    for (auto& id : moduleIds())
        setValue (state, FxChain::enabledParamId (id), chosen.contains (id) ? 1.0f : 0.0f);

    // Name: up to two describing words, then the space ("Vox" when there's none).
    while (adjectives.size() > 2)
        adjectives.remove (rng.nextInt (adjectives.size()));
    if (nouns.isEmpty())
        nouns.add ("Vox");
    adjectives.addArray (nouns);
    return adjectives.joinIntoString (" ");
}
