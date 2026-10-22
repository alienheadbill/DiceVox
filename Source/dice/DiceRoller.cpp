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
#include "modules/AlienModule.h"

#include <array>

namespace
{
    // Effects, in chain order (same order as DiceRoller::moduleIds()).
    enum Module { filter, pitch, drive, lofi, alien, chorus, chop, reverse, delay, reverb, numModules };

    // Styles, in Style-parameter order (same order as DiceRoller::styleNames()).
    enum Style { anyVibe, dreamyPop, darkToyBox, pitchedRage, lofiBedroom, glitch, area51, numStyles };

    // Intensity, in level order (same order as DiceRoller::levelNames()):
    // where in each setting's range a roll tends to land, and how many effects.
    constexpr float levelBias[]    = { 0.1f, 0.5f, 0.9f };
    constexpr int   levelMinCount[] = { 1, 2, 3 };
    constexpr int   levelMaxCount[] = { 2, 4, 5 };

    // Free-form rolls (part of Any Vibe): how often each effect comes up.
    constexpr float freeFormWeights[numModules] = {
        //  filter pitch drive lofi alien chorus chop reverse delay reverb
            0.8f,  0.6f, 0.7f, 0.5f, 0.4f, 0.7f,  0.4f, 0.4f,   1.0f, 1.3f };

    // Ways to set an effect. "any" = the style's usual mix of options.
    enum class AlienFeel  { any, saucer, android, transmission };
    enum class PitchFeel  { any, octaveDown, octaveUp, octaveEither, harmony, wholeUp, wholeDown, uncanny };
    enum class DelayFeel  { any, slap, dotted, throwIt, rhythmic };
    enum class ReverbFeel { any, room, space, hall };
    enum class LoFiFeel   { any, tape, crush, phone };
    enum class ChopFeel   { any, fast, groove };

    // Filter shapes (indices used by recipes and styles).
    enum { telephone, radio, thin, muffled, airy, numFilterShapes };

    struct FilterShape { const char* name; float lowCutLo, lowCutHi, highCutLo, highCutHi, resonanceLo, resonanceHi; };
    constexpr FilterShape filterShapes[numFilterShapes] = {
        { "Telephone", 250.0f,  500.0f, 2500.0f,  4000.0f, 10.0f, 50.0f },
        { "Radio",     500.0f,  900.0f, 3000.0f,  5000.0f, 30.0f, 70.0f },
        { "Thin",      400.0f, 1200.0f, 12000.0f, 20000.0f, 0.0f, 30.0f },
        { "Muffled",    40.0f,  100.0f,  800.0f,  2000.0f,  0.0f, 40.0f },
        { "Airy",      100.0f,  250.0f, 14000.0f, 20000.0f, 0.0f, 15.0f },
    };

    // How often each style picks each filter shape when a recipe doesn't say.
    constexpr float filterShapeWeights[numStyles][numFilterShapes] = {
        // Telephone Radio Thin Muffled Airy
        { 1, 1, 1, 1, 0 }, // Any Vibe
        { 0, 0, 1, 0, 3 }, // Dreamy Pop
        { 2, 1, 0, 2, 0 }, // Dark Toy Box
        { 1, 2, 1, 0, 0 }, // Pitched Rage
        { 1, 0, 0, 3, 0 }, // Lo-Fi Bedroom
        { 1, 1, 1, 0, 0 }, // Glitch
        { 1, 3, 0, 1, 0 }, // Area 51
    };

    // A recipe: one recognisable sound within a style. Its two core effects
    // always come (the first one even on LOW); the extra weights decide which
    // other effects join at MED / HIGH; the feels say how to set them.
    struct Recipe
    {
        Style style;
        int core[2];
        float extraWeights[numModules]; // filter pitch drive lofi alien chorus chop reverse delay reverb
        int filterShape;                // -1 = the style's usual shapes
        PitchFeel pitchFeel;
        DelayFeel delayFeel;
        ReverbFeel reverbFeel;
        LoFiFeel lofiFeel;
        ChopFeel chopFeel;
        AlienFeel alienFeel;
    };

    using PF = PitchFeel;
    using DF = DelayFeel;
    using RF = ReverbFeel;
    using LF = LoFiFeel;
    using CF = ChopFeel;
    using AF = AlienFeel;

    constexpr Recipe recipes[] = {
        //                                 filter pitch drive lofi  alien chorus chop reverse delay reverb
        // Dreamy Pop: airy, polished, wide.
        { dreamyPop, { reverb, chorus }, { 0.6f, 0.8f, 0.3f, 0.0f, 0.1f, 0.0f, 0.0f, 0.3f, 1.0f, 0.0f }, airy, PF::octaveDown,   DF::dotted,   RF::hall,  LF::any,   CF::any, AF::saucer },  // airy double
        { dreamyPop, { reverb, delay },  { 0.5f, 0.3f, 0.2f, 0.0f, 0.1f, 1.0f, 0.0f, 0.4f, 0.0f, 0.0f }, airy, PF::octaveEither, DF::throwIt,  RF::space, LF::any,   CF::any, AF::saucer },  // echo throw
        { dreamyPop, { chorus, pitch },  { 0.4f, 0.0f, 0.3f, 0.0f, 0.1f, 0.0f, 0.0f, 0.3f, 0.8f, 1.5f }, thin, PF::octaveUp,     DF::dotted,   RF::hall,  LF::any,   CF::any, AF::saucer },  // glossy stack

        // Dark Toy Box: creepy-cute, crunchy, eerie.
        { darkToyBox, { lofi, pitch },   { 0.5f, 0.0f, 0.2f, 0.0f, 0.3f, 0.4f, 0.2f, 0.6f, 0.4f, 1.2f }, -1,        PF::harmony,      DF::any,  RF::room, LF::tape,  CF::any, AF::saucer },       // music box
        { darkToyBox, { lofi, filter },  { 0.0f, 0.6f, 0.3f, 0.0f, 0.4f, 0.3f, 0.2f, 0.8f, 0.5f, 0.8f }, telephone, PF::octaveEither, DF::slap, RF::any,  LF::crush, CF::any, AF::transmission }, // creepy phone
        { darkToyBox, { pitch, reverb }, { 0.6f, 0.0f, 0.2f, 0.6f, 0.3f, 0.8f, 0.1f, 0.4f, 0.4f, 0.0f }, muffled,   PF::uncanny,      DF::any,  RF::room, LF::any,   CF::any, AF::android },      // uncanny whisper

        // Pitched Rage: the whole vocal pitched, or stacked octaves, with grit and throws.
        { pitchedRage, { pitch, delay }, { 0.4f, 0.0f, 1.0f, 0.3f, 0.2f, 0.3f, 0.6f, 0.3f, 0.0f, 0.5f }, radio, PF::wholeUp,      DF::rhythmic, RF::any,   LF::any, CF::groove, AF::saucer },  // chipmunk
        { pitchedRage, { pitch, drive }, { 0.6f, 0.0f, 0.0f, 0.3f, 0.4f, 0.3f, 0.4f, 0.4f, 0.8f, 1.0f }, radio, PF::wholeDown,    DF::throwIt,  RF::space, LF::any, CF::any,    AF::android }, // demon
        { pitchedRage, { pitch, chop },  { 0.3f, 0.0f, 0.8f, 0.3f, 0.2f, 0.4f, 0.0f, 0.3f, 1.0f, 0.6f }, -1,    PF::octaveEither, DF::rhythmic, RF::any,   LF::any, CF::groove, AF::any },     // octave stack

        // Lo-Fi Bedroom: worn tape, phone memos, dusty rooms.
        { lofiBedroom, { lofi, reverb }, { 0.8f, 0.3f, 0.6f, 0.0f, 0.1f, 0.8f, 0.1f, 0.3f, 0.4f, 0.0f }, muffled,   PF::octaveDown, DF::any,  RF::room, LF::tape,  CF::any, AF::saucer },       // tape demo
        { lofiBedroom, { lofi, filter }, { 0.0f, 0.2f, 0.4f, 0.0f, 0.3f, 0.4f, 0.1f, 0.3f, 0.5f, 0.6f }, telephone, PF::octaveDown, DF::slap, RF::room, LF::phone, CF::any, AF::transmission }, // phone memo
        { lofiBedroom, { lofi, drive },  { 0.6f, 0.4f, 0.0f, 0.0f, 0.1f, 0.6f, 0.2f, 0.3f, 0.4f, 1.0f }, muffled,   PF::octaveDown, DF::any,  RF::room, LF::crush, CF::any, AF::saucer },       // dusty room

        // Glitch: stutters, reverse swells, pitch jumps.
        { glitch, { chop, delay },    { 0.6f, 0.8f, 0.4f, 0.8f, 0.6f, 0.2f, 0.0f, 0.6f, 0.0f, 0.4f }, -1,    PF::any,          DF::rhythmic, RF::any,  LF::crush, CF::fast,   AF::any },    // stutter
        { glitch, { reverse, reverb },{ 0.4f, 0.8f, 0.2f, 0.4f, 0.5f, 0.4f, 0.6f, 0.0f, 0.6f, 0.0f }, -1,    PF::octaveEither, DF::any,      RF::hall, LF::any,   CF::any,    AF::saucer }, // reverse swell
        { glitch, { chop, pitch },    { 0.4f, 0.0f, 0.3f, 0.6f, 0.5f, 0.2f, 0.0f, 0.8f, 0.6f, 0.4f }, radio, PF::harmony,      DF::any,      RF::any,  LF::any,   CF::groove, AF::any },    // pitch jumps

        // Area 51: transmissions, androids and the mothership.
        { area51, { pitch, reverb },  { 0.3f, 0.0f, 0.2f, 0.2f, 0.6f, 1.2f, 0.1f, 0.4f, 0.6f, 0.0f }, airy,  PF::octaveDown,   DF::dotted,   RF::hall,  LF::any,   CF::any,    AF::saucer },       // mothership choir
        { area51, { alien, filter },  { 0.0f, 0.4f, 0.3f, 0.6f, 0.0f, 0.2f, 0.3f, 0.3f, 1.0f, 0.5f }, radio, PF::octaveEither, DF::slap,     RF::space, LF::crush, CF::any,    AF::transmission }, // crop circle radio
        { area51, { alien, delay },   { 0.2f, 0.8f, 0.6f, 0.2f, 0.0f, 0.3f, 0.5f, 0.3f, 0.0f, 0.5f }, -1,    PF::wholeDown,    DF::rhythmic, RF::any,   LF::any,   CF::groove, AF::android },      // android abduction
    };
    constexpr int numRecipes = (int) std::size (recipes);

    // Any Vibe: this share of rolls is free-form; the rest borrow a recipe from any style.
    constexpr float freeFormChance = 0.25f;

    // Remembers the last recipe (in the state tree) so a roll never repeats it.
    const juce::Identifier lastRecipeProperty { "diceLastRecipe" };

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

    // Random draws. Most of every range is reachable on every level; the level
    // nudges where they land (LOW toward the gentle end, HIGH toward the wild).
    struct Draw
    {
        juce::Random& rng;
        float bias;

        float t() const                          { return 0.65f * rng.nextFloat() + 0.35f * bias; }
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

    // Picks a recipe for the style (never the last one when there's a choice),
    // or -1 for a free-form Any Vibe roll.
    int pickRecipe (int style, int lastRecipe, juce::Random& rng)
    {
        if (style == anyVibe && rng.nextFloat() < freeFormChance)
            return -1;

        juce::Array<int> candidates;
        for (int r = 0; r < numRecipes; ++r)
            if ((style == anyVibe || recipes[r].style == style) && r != lastRecipe)
                candidates.add (r);

        if (candidates.isEmpty())
            return lastRecipe;
        return candidates[rng.nextInt (candidates.size())];
    }
}

const juce::StringArray& DiceRoller::moduleIds()
{
    static const juce::StringArray ids { "filter", "pitch", "drive", "lofi", "alien", "chorus", "chop", "reverse", "delay", "reverb" };
    return ids;
}

const juce::StringArray& DiceRoller::styleNames()
{
    static const juce::StringArray names { "Any Vibe", "Dreamy Pop", "Dark Toy Box", "Pitched Rage", "Lo-Fi Bedroom", "Glitch", "Area 51" };
    return names;
}

const juce::StringArray& DiceRoller::levelNames()
{
    static const juce::StringArray names { "Low", "Medium", "High" };
    return names;
}

int DiceRoller::numRecipesFor (int style)
{
    int n = 0;
    for (auto& r : recipes)
        n += (style == anyVibe || r.style == style) ? 1 : 0;
    return n;
}

juce::String DiceRoller::roll (juce::AudioProcessorValueTreeState& state, juce::Random& rng)
{
    const auto& ids = moduleIds();
    jassert (ids.size() == numModules);

    const int level = juce::jlimit (0, 2, juce::roundToInt (valueOf (state, ParamIDs::diceLevel)));
    const int requestedStyle = juce::jlimit (0, numStyles - 1, juce::roundToInt (valueOf (state, ParamIDs::diceStyle)));
    const Draw draw { rng, levelBias[level] };

    const int lastRecipe = (int) state.state.getProperty (lastRecipeProperty, -1);
    const int recipeIndex = pickRecipe (requestedStyle, lastRecipe, rng);
    const Recipe* recipe = recipeIndex >= 0 ? &recipes[recipeIndex] : nullptr;
    state.state.setProperty (lastRecipeProperty, recipeIndex, nullptr);

    // Settings follow the recipe's own style (Any Vibe borrows recipes from all styles).
    const int style = recipe != nullptr ? (int) recipe->style : (int) anyVibe;

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

    const int total = levelMinCount[level] + rng.nextInt (levelMaxCount[level] - levelMinCount[level] + 1);
    int slots = juce::jmax (0, total - lockedOn);

    if (recipe != nullptr)
    {
        for (int core : recipe->core)
        {
            if (slots > 0 && ! locked[(size_t) core] && ! chosen[(size_t) core])
            {
                chosen[(size_t) core] = true;
                --slots;
            }
        }
    }

    const float* weights = recipe != nullptr ? recipe->extraWeights : freeFormWeights;
    for (; slots > 0; --slots)
    {
        float sum = 0.0f;
        for (int m = 0; m < numModules; ++m)
            if (! locked[(size_t) m] && ! chosen[(size_t) m])
                sum += weights[m];
        if (sum <= 0.0f)
            break;

        float pick = rng.nextFloat() * sum;
        int k = -1;
        for (int m = 0; m < numModules; ++m)
        {
            if (locked[(size_t) m] || chosen[(size_t) m] || weights[m] <= 0.0f)
                continue;
            k = m;
            pick -= weights[m];
            if (pick < 0.0f)
                break;
        }
        if (k < 0)
            break;
        chosen[(size_t) k] = true;
    }

    // 2. Settings for each chosen effect, in chain order (so the name reads well).
    juce::StringArray adjectives, nouns;

    if (chosen[filter])
    {
        const int shapeIndex = recipe != nullptr && recipe->filterShape >= 0
                                 ? recipe->filterShape
                                 : draw.weighted (filterShapeWeights[style], numFilterShapes);
        const auto& s = filterShapes[shapeIndex];
        // Further toward HIGH = a narrower, more obvious band.
        setValue (state, FilterModule::lowCutId,    draw.logRange (s.lowCutLo, s.lowCutHi));
        setValue (state, FilterModule::highCutId,   draw.logRange (s.highCutHi, s.highCutLo));
        setValue (state, FilterModule::resonanceId, draw.range (s.resonanceLo, s.resonanceHi));
        adjectives.add (s.name);
    }

    if (chosen[pitch])
    {
        float shift = 0.0f, mix = 0.0f;
        const auto feel = recipe != nullptr ? recipe->pitchFeel : PitchFeel::any;
        switch (feel)
        {
            case PitchFeel::octaveDown:   shift = -12.0f;                               mix = draw.range (15, 45);  break;
            case PitchFeel::octaveUp:     shift = 12.0f;                                mix = draw.range (12, 35);  break;
            case PitchFeel::octaveEither: shift = draw.oneOf ({ -12, 12 });             mix = draw.range (20, 55);  break;
            case PitchFeel::harmony:      shift = draw.oneOf ({ -7, -5, 5, 7, 12 });    mix = draw.range (30, 65);  break;
            case PitchFeel::wholeUp:      shift = draw.oneOf ({ 3, 4, 5, 7 });          mix = 100.0f;               break;
            case PitchFeel::wholeDown:    shift = draw.oneOf ({ -3, -4, -5, -7 });      mix = 100.0f;               break;
            case PitchFeel::uncanny:      shift = draw.oneOf ({ -4, -3, -2, 2, 3, 4 }); mix = 100.0f;               break;
            case PitchFeel::any:
            default:
                if (draw.chance (0.15f + 0.15f * (float) level)) { shift = draw.oneOf ({ -5, -3, 3, 5 }); mix = 100.0f; }
                else                                             { shift = draw.oneOf ({ -12, -7, -5, 5, 7, 12 }); mix = draw.range (20, 65); }
                break;
        }
        setValue (state, PitchModule::shiftId, shift);
        setValue (state, PitchModule::mixId, mix);

        if (mix >= 90.0f)                 adjectives.add (shift >= 5.0f ? "Chipmunk" : shift > 0.0f ? "Pitched" : shift <= -5.0f ? "Demon" : "Uncanny");
        else if (std::abs (shift) >= 12)  adjectives.add (shift > 0.0f ? "Octave" : "Deep");
        else                              adjectives.add ("Harmony");
    }

    if (chosen[drive])
    {
        const float amount = style == dreamyPop   ? draw.range (5, 40)
                           : style == pitchedRage ? draw.range (30, 95)
                           : style == lofiBedroom ? draw.range (15, 65)
                                                  : draw.range (10, 85);
        setValue (state, DriveModule::amountId, amount);
        setValue (state, DriveModule::toneId, draw.uniform (25, 95));
        adjectives.add (amount < 45.0f ? "Warm" : "Gritty");
    }

    if (chosen[lofi])
    {
        float crush = 0.0f, wobble = 0.0f, mix = 0.0f;
        switch (recipe != nullptr ? recipe->lofiFeel : LoFiFeel::any)
        {
            case LoFiFeel::tape:  crush = draw.range (5, 50);  wobble = draw.range (45, 100); mix = draw.range (60, 100); break;
            case LoFiFeel::crush: crush = draw.range (55, 98); wobble = draw.range (0, 30);   mix = draw.range (50, 100); break;
            case LoFiFeel::phone: crush = draw.range (35, 75); wobble = draw.range (0, 25);   mix = 100.0f;               break;
            case LoFiFeel::any:
            default:              crush = draw.range (15, 90); wobble = draw.range (0, 80);   mix = draw.range (40, 100); break;
        }
        setValue (state, LoFiModule::crushId, crush);
        setValue (state, LoFiModule::wobbleId, wobble);
        setValue (state, LoFiModule::mixId, mix);
        adjectives.add (wobble > 50.0f ? "Tape" : crush > 70.0f ? "Crushed" : "Dusty");
    }

    if (chosen[alien])
    {
        auto feel = recipe != nullptr ? recipe->alienFeel : AlienFeel::any;
        if (feel == AlienFeel::any)
            feel = (AlienFeel) (1 + draw.weighted ({ 1, 1, 1 }));

        int mode = AlienModule::saucer;
        float amount = 0.0f, mix = 0.0f;
        switch (feel)
        {
            case AlienFeel::android:      mode = AlienModule::android;      amount = draw.range (5, 75);  mix = draw.range (35, 85);  break;
            case AlienFeel::transmission: mode = AlienModule::transmission; amount = draw.range (25, 95); mix = draw.range (65, 100); break;
            case AlienFeel::saucer:
            case AlienFeel::any:
            default:                      mode = AlienModule::saucer;       amount = draw.range (2, 60);  mix = draw.range (20, 60);  break;
        }
        setValue (state, AlienModule::modeId, (float) mode);
        setValue (state, AlienModule::amountId, amount);
        setValue (state, AlienModule::mixId, mix);
        adjectives.add (mode == AlienModule::android ? "Android" : mode == AlienModule::transmission ? "Beamed" : "Saucer");
    }

    if (chosen[chorus])
    {
        const float depth = style == dreamyPop ? draw.range (30, 85) : draw.range (20, 90);
        setValue (state, ChorusModule::rateId, draw.logRange (0.15f, 3.5f));
        setValue (state, ChorusModule::depthId, depth);
        adjectives.add (depth < 50.0f ? "Doubled" : "Wide");
    }

    if (chosen[chop])
    {
        // Indices into ChopModule::rateNames(): 1/32, 1/16, 1/16T, 1/8, 1/8T, 1/4
        int rate = 1;
        switch (recipe != nullptr ? recipe->chopFeel : ChopFeel::any)
        {
            case ChopFeel::fast:   rate = draw.weighted ({ 3, 1, 2, 0, 0, 0 }); break;
            case ChopFeel::groove: rate = draw.weighted ({ 0, 3, 1, 2, 1, 0.5f }); break;
            case ChopFeel::any:
            default:               rate = draw.weighted ({ 1, 3, 1, 2, 1, 1 }); break;
        }
        setValue (state, ChopModule::rateId, (float) rate);
        setValue (state, ChopModule::depthId, draw.range (40, 100));
        adjectives.add (rate == 0 || rate == 2 ? "Stutter" : "Choppy");
    }

    if (chosen[reverse])
    {
        // Indices into ReverseModule::lengthNames(): 1/16, 1/8, 1/4, 1/2
        setValue (state, ReverseModule::lengthId, (float) draw.weighted ({ 1, 2, 3, 1.5f }));
        setValue (state, ReverseModule::mixId, draw.range (20, 75));
        adjectives.add ("Backwards");
    }

    if (chosen[delay])
    {
        // Indices into DelayModule::divisionNames(): 1/16, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/4D, 1/2.
        int division = 0;
        float mix = draw.range (12, 50);
        switch (recipe != nullptr ? recipe->delayFeel : DelayFeel::any)
        {
            case DelayFeel::slap:     division = draw.weighted ({ 3, 1, 0, 0, 0, 0, 0, 0 }); break;
            case DelayFeel::dotted:   division = draw.weighted ({ 0, 0, 0, 3, 0, 0, 2, 0 }); break;
            case DelayFeel::throwIt:  division = draw.weighted ({ 0, 0, 0, 0, 0, 3, 1, 2 }); mix = draw.range (30, 60); break;
            case DelayFeel::rhythmic: division = draw.weighted ({ 0, 2, 3, 1, 2, 0, 0, 0 }); break;
            case DelayFeel::any:
            default:                  division = draw.weighted ({ 1, 1, 2, 3, 1, 2, 1, 1 }); break;
        }
        setValue (state, DelayModule::timeId, (float) division);
        setValue (state, DelayModule::feedbackId, draw.range (10, 70));
        setValue (state, DelayModule::mixId, mix);
        nouns.add (division == 0 ? "Ping" : division == 7 ? "Beam" : "Echo");
    }

    if (chosen[reverb])
    {
        float size = 0.0f, mix = 0.0f;
        switch (recipe != nullptr ? recipe->reverbFeel : ReverbFeel::any)
        {
            case ReverbFeel::room:  size = draw.range (20, 50); mix = draw.range (12, 40); break;
            case ReverbFeel::space: size = draw.range (45, 80); mix = draw.range (15, 50); break;
            case ReverbFeel::hall:  size = draw.range (70, 98); mix = draw.range (20, 55); break;
            case ReverbFeel::any:
            default:                size = draw.range (25, 95); mix = draw.range (12, 50); break;
        }
        setValue (state, ReverbModule::sizeId, size);
        setValue (state, ReverbModule::mixId, mix);
        nouns.add (size < 50.0f ? "Pod" : size < 78.0f ? "Orbit" : "Mothership");
    }

    // 3. Switch the chosen effects on and the other unlocked ones off.
    for (int m = 0; m < numModules; ++m)
        if (! locked[(size_t) m])
            setValue (state, FxChain::enabledParamId (ids[m]), chosen[(size_t) m] ? 1.0f : 0.0f);

    // Name: up to two describing words (sometimes one from outer space), then
    // the echo / space ("Signal", "Vox" or "Transmission" when there's none).
    static const juce::StringArray cosmicWords { "Martian", "Cosmic", "Nebula", "Abducted", "Zero-G",
                                                 "Orbital", "Ultraviolet", "Interstellar", "Lunar", "Galactic" };
    while (adjectives.size() > 2)
        adjectives.remove (rng.nextInt (adjectives.size()));
    if (draw.chance (0.45f))
    {
        if (adjectives.size() == 2)
            adjectives.remove (rng.nextInt (2));
        adjectives.insert (0, cosmicWords[rng.nextInt (cosmicWords.size())]);
    }
    if (nouns.isEmpty())
    {
        static const juce::StringArray loneNouns { "Signal", "Vox", "Transmission" };
        nouns.add (loneNouns[rng.nextInt (loneNouns.size())]);
    }
    adjectives.addArray (nouns);
    return adjectives.joinIntoString (" ");
}
