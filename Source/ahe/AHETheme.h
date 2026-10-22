#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AHEFonts.h"

// ---------------------------------------------------------------------------
// AHE Audio design tokens (shared by every AHE plug-in).
//
// Source of truth: design/ahe-ui-tokens.json (v0.2).
// Keep this file in sync with that JSON; plug-ins read colours, spacing and
// font roles from here instead of hard-coding them.
//
// Colour roles:
//   purple  = structure, selection, the main creative action, parameter values
//   green   = processing is actually ON (never used for anything decorative)
//   State is never communicated by colour alone: always pair with text/shape.
// ---------------------------------------------------------------------------
namespace ahe::theme
{
    namespace colour
    {
        inline const juce::Colour brandPurple   { 0xff8f32c7 };
        inline const juce::Colour alienGreen    { 0xff52be29 };
        inline const juce::Colour purpleSoft    { 0xffb47bdd };
        inline const juce::Colour greenSoft     { 0xff8fea65 };
        inline const juce::Colour canvas        { 0xff101014 };
        inline const juce::Colour panel         { 0xff1d1b26 };
        inline const juce::Colour raised        { 0xff282531 };
        inline const juce::Colour border        { 0xff383442 };
        inline const juce::Colour textPrimary   { 0xfff0eef6 };
        inline const juce::Colour textSecondary { 0xffa6a1b3 };
        inline const juce::Colour textMuted     { 0xff716d7d };
        inline const juce::Colour warning       { 0xfff07583 };
    }

    // Spacing scale (px at 100% size): 4, 8, 12, 16, 24, 32
    namespace space
    {
        inline constexpr int xs = 4, s = 8, m = 12, l = 16, xl = 24, xxl = 32;
    }

    // Corner radii (px): small controls, cards, hero panels
    namespace radius
    {
        inline constexpr float small = 9.0f, card = 14.0f, hero = 20.0f;
    }

    // Font roles, using the embedded AHE typefaces (see AHEFonts.h), so text
    // looks identical on every computer.
    namespace font
    {
        // Product names only (e.g. "DICEVOX"): Barlow Condensed ExtraBold Italic.
        inline juce::FontOptions display (float size)
        {
            return juce::FontOptions (ahe::fonts::display()).withHeight (size);
        }

        // Section labels ("INPUT", "QUICK"): Inter SemiBold, tracked.
        inline juce::FontOptions label (float size = 11.0f)
        {
            return juce::FontOptions (ahe::fonts::uiSemiBold()).withHeight (size).withKerningFactor (0.08f);
        }

        // Body text: Inter Regular.
        inline juce::FontOptions body (float size = 13.0f)
        {
            return juce::FontOptions (ahe::fonts::uiRegular()).withHeight (size);
        }

        // Values and read-outs: Inter SemiBold.
        inline juce::FontOptions value (float size = 13.0f)
        {
            return juce::FontOptions (ahe::fonts::uiSemiBold()).withHeight (size);
        }
    }

    // Component property used to pick a styling variant in AHELookAndFeel.
    inline const juce::Identifier roleProperty { "ahe.role" };

    namespace role
    {
        inline const juce::String tab     { "tab" };     // header navigation tab
        inline const juce::String bypass  { "bypass" };  // processing / bypassed (or on / off) switch
        inline const juce::String primary { "primary" }; // the main creative action (e.g. ROLL)
        inline const juce::String lock    { "lock" };    // small padlock toggle (closed = locked)
        inline const juce::String segment { "segment" }; // one option of a segmented choice (lit = selected)
    }
}
