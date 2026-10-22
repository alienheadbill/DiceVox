#include "FactoryPresets.h"

namespace
{
    struct Setting { const char* id; float value; };

    struct Preset
    {
        const char* name;
        Setting settings[16]; // ends at the first entry with no id
    };

    // Choice parameters hold the option's index:
    //   delay_time   0 1/16, 1 1/8T, 2 1/8, 3 1/8D, 4 1/4T, 5 1/4, 6 1/4D, 7 1/2
    //   reverse_length 0 1/16, 1 1/8, 2 1/4, 3 1/2
    //   chop_rate    0 1/32, 1 1/16, 2 1/16T, 3 1/8, 4 1/8T, 5 1/4
    //   alien_mode   0 Saucer, 1 Android, 2 Transmission
    constexpr Preset presets[] = {
        { "Mothership Choir", {
            { "pitch_on", 1 }, { "pitch_shift", -12 }, { "pitch_mix", 35 },
            { "chorus_on", 1 }, { "chorus_rate", 0.5f }, { "chorus_depth", 60 },
            { "delay_on", 1 }, { "delay_time", 6 }, { "delay_feedback", 35 }, { "delay_mix", 20 },
            { "reverb_on", 1 }, { "reverb_size", 92 }, { "reverb_mix", 40 } } },

        { "Crop Circle Phone", {
            { "filter_on", 1 }, { "filter_lowcut", 300 }, { "filter_highcut", 3400 }, { "filter_resonance", 30 },
            { "lofi_on", 1 }, { "lofi_crush", 55 }, { "lofi_wobble", 20 }, { "lofi_mix", 100 },
            { "reverse_on", 1 }, { "reverse_length", 1 }, { "reverse_mix", 35 },
            { "reverb_on", 1 }, { "reverb_size", 40 }, { "reverb_mix", 20 } } },

        { "Area 51 Transmission", {
            { "alien_on", 1 }, { "alien_mode", 2 }, { "alien_amount", 60 }, { "alien_mix", 100 },
            { "delay_on", 1 }, { "delay_time", 2 }, { "delay_feedback", 45 }, { "delay_mix", 30 },
            { "reverb_on", 1 }, { "reverb_size", 55 }, { "reverb_mix", 18 } } },

        { "Android Abduction", {
            { "alien_on", 1 }, { "alien_mode", 1 }, { "alien_amount", 40 }, { "alien_mix", 70 },
            { "pitch_on", 1 }, { "pitch_shift", -3 }, { "pitch_mix", 100 },
            { "drive_on", 1 }, { "drive_amount", 50 }, { "drive_tone", 55 },
            { "delay_on", 1 }, { "delay_time", 0 }, { "delay_feedback", 20 }, { "delay_mix", 25 } } },

        { "Zero-G Float", {
            { "filter_on", 1 }, { "filter_lowcut", 160 }, { "filter_highcut", 16000 }, { "filter_resonance", 5 },
            { "chorus_on", 1 }, { "chorus_rate", 0.3f }, { "chorus_depth", 80 },
            { "delay_on", 1 }, { "delay_time", 7 }, { "delay_feedback", 60 }, { "delay_mix", 35 },
            { "reverb_on", 1 }, { "reverb_size", 98 }, { "reverb_mix", 50 } } },

        { "Martian Chipmunk", {
            { "pitch_on", 1 }, { "pitch_shift", 5 }, { "pitch_mix", 100 },
            { "drive_on", 1 }, { "drive_amount", 40 }, { "drive_tone", 70 },
            { "delay_on", 1 }, { "delay_time", 2 }, { "delay_feedback", 40 }, { "delay_mix", 25 },
            { "reverb_on", 1 }, { "reverb_size", 45 }, { "reverb_mix", 15 } } },

        { "Saucer Wobble", {
            { "alien_on", 1 }, { "alien_mode", 0 }, { "alien_amount", 25 }, { "alien_mix", 40 },
            { "lofi_on", 1 }, { "lofi_crush", 15 }, { "lofi_wobble", 90 }, { "lofi_mix", 60 },
            { "reverb_on", 1 }, { "reverb_size", 60 }, { "reverb_mix", 30 } } },

        { "Nebula Swell", {
            { "reverse_on", 1 }, { "reverse_length", 2 }, { "reverse_mix", 60 },
            { "pitch_on", 1 }, { "pitch_shift", 12 }, { "pitch_mix", 20 },
            { "reverb_on", 1 }, { "reverb_size", 90 }, { "reverb_mix", 45 } } },

        { "Signal Lost", {
            { "chop_on", 1 }, { "chop_rate", 0 }, { "chop_depth", 85 },
            { "lofi_on", 1 }, { "lofi_crush", 80 }, { "lofi_wobble", 10 }, { "lofi_mix", 100 },
            { "alien_on", 1 }, { "alien_mode", 2 }, { "alien_amount", 45 }, { "alien_mix", 50 },
            { "delay_on", 1 }, { "delay_time", 1 }, { "delay_feedback", 30 }, { "delay_mix", 25 } } },

        { "UFO Landing", {
            { "pitch_on", 1 }, { "pitch_shift", -12 }, { "pitch_mix", 50 },
            { "drive_on", 1 }, { "drive_amount", 60 }, { "drive_tone", 50 },
            { "alien_on", 1 }, { "alien_mode", 0 }, { "alien_amount", 8 }, { "alien_mix", 30 },
            { "reverb_on", 1 }, { "reverb_size", 70 }, { "reverb_mix", 30 } } },
    };
}

int FactoryPresets::count()
{
    return (int) std::size (presets);
}

juce::String FactoryPresets::name (int index)
{
    return juce::isPositiveAndBelow (index, count()) ? juce::String (presets[index].name) : juce::String();
}

std::unique_ptr<juce::XmlElement> FactoryPresets::create (int index)
{
    if (! juce::isPositiveAndBelow (index, count()))
        return nullptr;

    const auto& preset = presets[index];
    auto xml = std::make_unique<juce::XmlElement> ("DiceVoxPreset");
    xml->setAttribute ("formatVersion", 1);
    xml->setAttribute ("rollName", preset.name);

    for (const auto& s : preset.settings)
    {
        if (s.id == nullptr)
            break;
        auto* child = xml->createNewChildElement ("PARAM");
        child->setAttribute ("id", s.id);
        child->setAttribute ("value", (double) s.value);
    }
    return xml;
}
