#include "PresetStore.h"
#include "Params.h"

namespace
{
    constexpr auto rootTag = "DiceVoxPreset";
    constexpr int formatVersion = 1;
}

PresetStore::PresetStore (juce::File presetFolder)
    : folder (std::move (presetFolder))
{
}

juce::File PresetStore::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("AHE Audio").getChildFile ("DiceVox").getChildFile ("Presets");
}

juce::Array<juce::File> PresetStore::list() const
{
    auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);

    struct ByName
    {
        static int compareElements (const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension());
        }
    };
    ByName sorter;
    files.sort (sorter);
    return files;
}

juce::File PresetStore::fileFor (const juce::String& name) const
{
    auto legal = juce::File::createLegalFileName (name.trim());
    if (legal.isEmpty())
        legal = "Untitled";
    return folder.getChildFile (legal + extension);
}

bool PresetStore::isPresetParameter (const juce::String& paramId)
{
    return paramId != ParamIDs::bypass && ! ParamIDs::isDiceControl (paramId);
}

juce::File PresetStore::save (const juce::AudioProcessorValueTreeState& state, const juce::String& name,
                              const juce::String& rollName) const
{
    juce::XmlElement xml (rootTag);
    xml.setAttribute ("formatVersion", formatVersion);
    xml.setAttribute ("rollName", rollName);

    for (auto* param : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr || ! isPresetParameter (ranged->getParameterID()))
            continue;

        auto* child = xml.createNewChildElement ("PARAM");
        child->setAttribute ("id", ranged->getParameterID());
        child->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
    }

    const auto file = fileFor (name);
    if (folder.createDirectory().failed() || ! xml.writeTo (file))
        return {};
    return file;
}

std::unique_ptr<juce::XmlElement> PresetStore::read (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (rootTag))
        return nullptr;
    return xml;
}
