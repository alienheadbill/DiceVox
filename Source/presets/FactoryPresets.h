#pragma once

#include <juce_core/juce_core.h>

// ---------------------------------------------------------------------------
// FactoryPresets — the built-in "Alien Pack", compiled into the plug-in.
//
// Each one is the same XML a user preset file holds (see PresetStore), but
// only lists what it changes: everything else loads at its default, so every
// effect a preset doesn't name is off.
// ---------------------------------------------------------------------------
namespace FactoryPresets
{
    int count();
    juce::String name (int index);
    std::unique_ptr<juce::XmlElement> create (int index);
}
