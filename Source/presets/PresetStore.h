#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// PresetStore — the user's DiceVox presets, one file each.
//
// Folder:  Documents/AHE Audio/DiceVox/Presets  (created on first save)
// File:    <name>.dicevox, a small XML file:
//            <DiceVoxPreset formatVersion="1" rollName="DUSTY DEEP ECHO HALL">
//              <PARAM id="reverb_mix" value="31.5"/> ...
//            </DiceVoxPreset>
//
// A preset is the sound: every parameter except Bypass and the dice controls
// (intensity, style, locks), which are about how you roll, not what you hear.
// Parameters missing from an older preset load at their defaults, so presets
// keep working as effects are added.
// ---------------------------------------------------------------------------
class PresetStore
{
public:
    explicit PresetStore (juce::File folder = defaultFolder());

    static juce::File defaultFolder();
    static constexpr auto extension = ".dicevox";

    void setFolder (const juce::File& newFolder) { folder = newFolder; }
    const juce::File& getFolder() const noexcept { return folder; }

    // Preset files, sorted by name (case-insensitive).
    juce::Array<juce::File> list() const;

    // The file a preset name maps to (characters Windows can't use are removed).
    juce::File fileFor (const juce::String& name) const;

    // Writes the current sound; returns the file, or a non-existent file on failure.
    juce::File save (const juce::AudioProcessorValueTreeState& state, const juce::String& name,
                     const juce::String& rollName) const;

    // Is this parameter part of a preset's sound?
    static bool isPresetParameter (const juce::String& paramId);

    // Reads a preset file; nullptr if it isn't a DiceVox preset.
    static std::unique_ptr<juce::XmlElement> read (const juce::File& file);

    static juce::String nameOf (const juce::File& file) { return file.getFileNameWithoutExtension(); }

private:
    juce::File folder;
};
