#include "DiceRoller.h"
#include "Params.h"
#include "modules/FxChain.h"
#include "modules/FilterModule.h"
#include "modules/PitchModule.h"
#include "modules/DriveModule.h"
#include "modules/LoFiModule.h"
#include "modules/ChorusModule.h"
#include "modules/ChopModule.h"
#include "modules/ReverseModule.h"
#include "modules/DelayModule.h"
#include "modules/ReverbModule.h"

#include <array>

namespace
{
    // Effects, in chain order (same order as DiceRoller::moduleIds()).
    enum Module { filter, pitch, drive, lofi, chorus, chop, reverse, delay, reverb, numModules };

    // Styles, in Style-parameter order (same order as DiceRoller::styleNames()).
    enum Style { anyVibe, dreamyPop, darkToyBox, pitchedRage, lofiBedroom, glitch, numStyles };

    // How often each style picks each effect (relative weights).
    constexpr float styleWeights[numStyles][numModules] = {
        //  filter pitch  drive  lofi   chorus chop   reverse delay reverb
        {   0.8f,  0.6f,  0.7f,  0.5f,  0.7f,  0.4f,  0.4f,   1.0f, 1.3f }, // Any Vibe
        {   0.3f,  0.5f,  0.3f,  0.1f,  1.5f,  0.1f,  0.4f,   1.2f, 2.0f }, // Dreamy Pop
        {   1.4f,  1.4f,  0.4f,  1.5f,  0.6f,  0.3f,  0.8f,   0.6f, 1.2f }, // Dark Toy Box
        {   0.6f,  2.0f,  1.2f,  0.5f,  0.6f,  1.0f,  0.6f,   1.3f, 0.8f }, // Pitched Rage
        {   1.2f,  0.3f,  0.8f,  2.0f,  0.8f,  0.3f,  0.5f,   0.8f, 1.0f }, // Lo-Fi Bedroom
        {   0.8f,  1.2f,  0.4f,  1.0f,  0.4f,  2.0f,  2.0f,   1.0f, 0.6f }, // Glitch
    };

    // The one effect each style always brings (unless it is locked), -1 = none.
    constexpr int styleSignature[numStyles] = { -1, reverb, lofi, pitch, lofi, chop };

    struct FilterShape { const char* name; float lowCutLo, lowCutHi, highCutLo, highCutHi, resonanceLo, resonanceHi; };
    constexpr FilterShape filterShapes[] = {
        { "Telephone", 250.0f,  500.0f, 2500.0f,  4000.0f, 10.0f, 50.0f },
        { "Radio",     500.0f,  900.0f, 3000.0f,  5000.0f, 30.0f, 70.0f },
        { "Thin",      400.0f, 1200.0f, 12000.0f, 20000.0f, 0.0f, 30.0f },
        { "Muffled",    40.0f,  100.0f,  800.0f,  2000.0f,  0.0f, 40.0f },
        { "Airy",      100.0f,  250.0f, 14000.0f, 20000.0f, 0.0f, 15.0f },
    };
    constexpr int numFilterShapes = (int) std::size (filterShapes);

    // How often each style picks each filter shape.
    constexpr float filterShapeWeights[numStyles][numFilterShapes] = {
        // Telephone Radio Thin Muffled Airy
        { 1, 1, 1, 1, 0 }, // Any Vibe
        { 0, 0, 1, 0, 3 }, // Dreamy Pop
        { 2, 1, 0, 2, 0 }, // Dark Toy Box
        { 1, 2, 1, 0, 0 }, // Pitched Rage
        { 1, 0, 0, 3, 0 }, // Lo-Fi Bedroom
        { 1, 1, 1, 0, 0 }, // Glitch
    };

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

    float valueOf (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        auto* v = state.getRawParameterValue (id);
        jassert (v != nullptr);
        return v != nullptr ? v->load() : 0.0f;
    }

    // Random draws, steered by intensity: at 0 they land in the lower 40 % of
    // a range, at 1 in the upper 40 %, at 0.5 in the middle.
    struct Draw
    {
        juce::Random& rng;
        float intensity;

        float t() const                          { return 0.6f * intensity + 0.4f * rng.nextFloat(); }
        float range (float lo, float hi) const    { return lo + t() * (hi - lo); }
        float logRange (float lo, float hi) const { return lo * std::pow (hi / lo, t()); }
        float uniform (float lo, float hi) const  { return lo + rng.nextFloat() * (hi - lo); }
        bool  chance (float p) const              { return rng.nextFloat() < p; }

        float oneOf (std::initializer_list<float> items) const
        {
            return items.begin()[rng.nextInt ((int) items.size())];
        }

        int weighted (const float* weights, int count) const
        {
            float total = 0.0f;
            for (int i = 0; i < count; ++i)
                total += weights[i];

            float pick = rng.nextFloat() * total;
            for (int i = 0; i < count; ++i)
            {
                pick -= weights[i];
                if (pick < 0.0f && weights[i] > 0.0f)
                    return i;
            }
            // Rounding left a sliver: take the last option that can come up.
            for (int i = count - 1; i > 0; --i)
                if (weights[i] > 0.0f)
                    return i;
            return 0;
        }

        int weighted (std::initializer_list<float> weights) const
        {
            return weighted (weights.begin(), (int) weights.size());
        }
    };
}

const juce::StringArray& DiceRoller::moduleIds()
{
    static const juce::StringArray ids { "filter", "pitch", "drive", "lofi", "chorus", "chop", "reverse", "delay", "reverb" };
    return ids;
}

const juce::StringArray& DiceRoller::styleNames()
{
    static const juce::StringArray names { "Any Vibe", "Dreamy Pop", "Dark Toy Box", "Pitched Rage", "Lo-Fi Bedroom", "Glitch" };
    return names;
}

juce::String DiceRoller::roll (juce::AudioProcessorValueTreeState& state, juce::Random& rng)
{
    const auto& ids = moduleIds();
    jassert (ids.size() == numModules);

    const float intensity = juce::jlimit (0.0f, 1.0f, valueOf (state, ParamIDs::diceIntensity) / 100.0f);
    const int style = juce::jlimit (0, numStyles - 1, juce::roundToInt (valueOf (state, ParamIDs::diceStyle)));
    const Draw draw { rng, intensity };

    // 1. Which effects. Locked ones stay exactly as they are; locked effects
    //    that are on count toward the total.
    std::array<bool, numModules> locked {}, chosen {};
    int lockedOn = 0;
    for (int m = 0; m < numModules; ++m)
    {
        locked[(size_t) m] = valueOf (state, ParamIDs::diceLock (ids[m])) > 0.5f;
        if (locked[(size_t) m] && valueOf (state, FxChain::enabledParamId (ids[m])) > 0.5f)
            ++lockedOn;
    }

    const int total = juce::jlimit (1, 5, 1 + juce::roundToInt (2.0f * intensity) + rng.nextInt (3));
    int slots = juce::jmax (0, total - lockedOn);

    if (const int signature = styleSignature[style]; signature >= 0 && slots > 0 && ! locked[(size_t) signature])
    {
        chosen[(size_t) signature] = true;
        --slots;
    }

    for (; slots > 0; --slots)
    {
        float sum = 0.0f;
        for (int m = 0; m < numModules; ++m)
            if (! locked[(size_t) m] && ! chosen[(size_t) m])
                sum += styleWeights[style][m];
        if (sum <= 0.0f)
            break;

        float pick = rng.nextFloat() * sum;
        int k = -1;
        for (int m = 0; m < numModules; ++m)
        {
            if (locked[(size_t) m] || chosen[(size_t) m])
                continue;
            k = m;
            pick -= styleWeights[style][m];
            if (pick < 0.0f)
                break;
        }
        chosen[(size_t) k] = true;
    }

    // 2. Settings for each chosen effect, in chain order (so the name reads well).
    juce::StringArray adjectives, nouns;

    if (chosen[filter])
    {
        const auto& s = filterShapes[draw.weighted (filterShapeWeights[style], numFilterShapes)];
        // Higher intensity = a narrower, more obvious band.
        setValue (state, FilterModule::lowCutId,    draw.logRange (s.lowCutLo, s.lowCutHi));
        setValue (state, FilterModule::highCutId,   draw.logRange (s.highCutHi, s.highCutLo));
        setValue (state, FilterModule::resonanceId, draw.range (s.resonanceLo, s.resonanceHi));
        adjectives.add (s.name);
    }

    if (chosen[pitch])
    {
        float shift = 0.0f, mix = 0.0f;
        switch (style)
        {
            case dreamyPop:   shift = draw.oneOf ({ -12, 12 }); mix = draw.range (12, 35); break;
            case lofiBedroom: shift = -12.0f;                   mix = draw.range (15, 40); break;
            case glitch:      shift = draw.oneOf ({ -12, 12, -7, 7, 5 }); mix = draw.range (40, 100); break;
            case darkToyBox:
                if (draw.chance (0.5f)) { shift = draw.oneOf ({ -4, -3, 3, 4 }); mix = 100.0f; }   // uncanny, slightly off voice
                else                    { shift = draw.oneOf ({ -12, 12, 7 }); mix = draw.range (30, 60); }
                break;
            case pitchedRage:
                if (draw.chance (0.5f)) { shift = draw.oneOf ({ 3, 4, 5, -3, -4, -5 }); mix = 100.0f; } // the whole vocal pitched
                else                    { shift = draw.oneOf ({ -12, 12 }); mix = draw.range (35, 70); }
                break;
            default:
                if (draw.chance (0.2f + 0.3f * intensity)) { shift = draw.oneOf ({ -5, -3, 3, 5 }); mix = 100.0f; }
                else                                       { shift = draw.oneOf ({ -12, -7, -5, 5, 7, 12 }); mix = draw.range (20, 60); }
                break;
        }
        setValue (state, PitchModule::shiftId, shift);
        setValue (state, PitchModule::mixId, mix);

        if (mix >= 90.0f)                 adjectives.add (shift >= 5.0f ? "Chipmunk" : shift > 0.0f ? "Pitched" : shift <= -5.0f ? "Demon" : "Deep");
        else if (std::abs (shift) >= 12)  adjectives.add (shift > 0.0f ? "Octave" : "Deep");
        else                              adjectives.add ("Harmony");
    }

    if (chosen[drive])
    {
        const float amount = style == dreamyPop   ? draw.range (10, 35)
                           : style == pitchedRage ? draw.range (40, 90)
                           : style == lofiBedroom ? draw.range (20, 55)
                                                  : draw.range (15, 80);
        setValue (state, DriveModule::amountId, amount);
        setValue (state, DriveModule::toneId, draw.uniform (30, 90));
        adjectives.add (amount < 45.0f ? "Warm" : "Gritty");
    }

    if (chosen[lofi])
    {
        const float wobble = style == lofiBedroom ? draw.range (30, 90) : draw.range (5, 70);
        setValue (state, LoFiModule::crushId, style == lofiBedroom ? draw.range (35, 90) : draw.range (20, 85));
        setValue (state, LoFiModule::wobbleId, wobble);
        setValue (state, LoFiModule::mixId, draw.range (50, 100));
        adjectives.add (wobble > 50.0f ? "Tape" : "Dusty");
    }

    if (chosen[chorus])
    {
        const float depth = style == dreamyPop ? draw.range (35, 80) : draw.range (25, 85);
        setValue (state, ChorusModule::rateId, draw.logRange (0.2f, 2.5f));
        setValue (state, ChorusModule::depthId, depth);
        adjectives.add (depth < 50.0f ? "Doubled" : "Wide");
    }

    if (chosen[chop])
    {
        // Indices into ChopModule::rateNames(): 1/32, 1/16, 1/16T, 1/8, 1/8T, 1/4
        const int rate = style == glitch ? draw.weighted ({ 3, 2, 3, 1, 1, 0.5f })
                                         : draw.weighted ({ 1, 3, 1, 2, 1, 1 });
        setValue (state, ChopModule::rateId, (float) rate);
        setValue (state, ChopModule::depthId, draw.range (50, 100));
        adjectives.add (rate == 0 || rate == 2 ? "Stutter" : "Choppy");
    }

    if (chosen[reverse])
    {
        // Indices into ReverseModule::lengthNames(): 1/16, 1/8, 1/4, 1/2
        setValue (state, ReverseModule::lengthId, (float) draw.weighted ({ 1, 2, 3, 1 }));
        setValue (state, ReverseModule::mixId, draw.range (25, 70));
        adjectives.add ("Backwards");
    }

    if (chosen[delay])
    {
        // Indices into DelayModule::divisionNames(): 1/16, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/4D, 1/2.
        // 1/8, 1/8 dotted and 1/4 are the classic vocal throws.
        const int division = style == dreamyPop ? draw.weighted ({ 0, 0, 1, 3, 0, 3, 1, 0 })
                                                : draw.weighted ({ 1, 1, 2, 3, 1, 2, 1, 1 });
        setValue (state, DelayModule::timeId, (float) division);
        setValue (state, DelayModule::feedbackId, draw.range (15, 65));
        setValue (state, DelayModule::mixId, draw.range (15, 50));
        nouns.add (division == 0 ? "Slap" : division == 7 ? "Throw" : "Echo");
    }

    if (chosen[reverb])
    {
        const bool big = style == dreamyPop;
        const float size = big ? draw.range (65, 95) : draw.range (30, 95);
        setValue (state, ReverbModule::sizeId, size);
        setValue (state, ReverbModule::mixId, big ? draw.range (25, 55) : draw.range (15, 50));
        nouns.add (size < 50.0f ? "Room" : size < 78.0f ? "Space" : "Hall");
    }

    // 3. Switch the chosen effects on and the other unlocked ones off.
    for (int m = 0; m < numModules; ++m)
        if (! locked[(size_t) m])
            setValue (state, FxChain::enabledParamId (ids[m]), chosen[(size_t) m] ? 1.0f : 0.0f);

    // Name: up to two describing words, then the space ("Vox" when there's none).
    while (adjectives.size() > 2)
        adjectives.remove (rng.nextInt (adjectives.size()));
    if (nouns.isEmpty())
        nouns.add ("Vox");
    adjectives.addArray (nouns);
    return adjectives.joinIntoString (" ");
}
