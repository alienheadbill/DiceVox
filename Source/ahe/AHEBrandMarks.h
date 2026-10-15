#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ---------------------------------------------------------------------------
// AHE Audio brand marks, loaded from the approved SVGs in design/assets/
// (compiled into the plug-in as binary data, so nothing is read from disk or
// the network at runtime).
//
//   alienIcon()  primary mark: alien head (small sizes, plug-in header)
//   ufoIcon()    secondary motif: UFO with beam
//   wordmark()   the AHE letters only, taken as vector shapes from the approved
//                lockup artwork (compact maker mark; no SVG text is used)
//
// The full stacked lockup (design/assets/ahe-alien-horizontal.svg) is kept as
// a brand asset for installers, splash screens and marketing. It is not drawn
// inside the plug-in: its "AUDIO" is live SVG text, which JUCE's SVG reader
// does not render, and it is unreadably small at header size.
//
// Each call returns a fresh Drawable; callers own it. Returns nullptr only if
// the embedded SVG failed to parse (covered by the offline tests).
// ---------------------------------------------------------------------------
namespace ahe::brand
{
    std::unique_ptr<juce::Drawable> alienIcon();
    std::unique_ptr<juce::Drawable> ufoIcon();
    std::unique_ptr<juce::Drawable> wordmark();

    // Recolours every shape in a mark to a single tone, keeping each shape's own
    // transparency (e.g. a UFO beam at 24% stays at 24%). Used for marks shown on
    // unavailable controls, where brand green would wrongly signal "processing on".
    void makeMonochrome (juce::Drawable& mark, juce::Colour tone);
}
