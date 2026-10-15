#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ---------------------------------------------------------------------------
// AHE Audio typefaces, embedded in the plug-in so every computer renders the
// UI identically (no dependence on installed system fonts).
//
//   Inter (Regular, SemiBold)            UI text, labels and values
//   Barlow Condensed ExtraBold Italic    product names only (e.g. "DICEVOX")
//
// Both are SIL Open Font License 1.1; the files and licence texts live in
// design/fonts/ and are embedded unmodified.
// ---------------------------------------------------------------------------
namespace ahe::fonts
{
    juce::Typeface::Ptr uiRegular();
    juce::Typeface::Ptr uiSemiBold();
    juce::Typeface::Ptr display();
}
