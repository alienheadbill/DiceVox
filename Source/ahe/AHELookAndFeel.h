#pragma once

#include "AHETheme.h"

namespace ahe
{
    // -----------------------------------------------------------------------
    // AHELookAndFeel — the shared AHE Audio control style.
    //
    // Flat, crisp vector drawing on a dark base. Every interactive control
    // shows distinct hover, drag, keyboard-focus and disabled states, and no
    // state relies on colour alone.
    //
    // Variants are chosen with the component property theme::roleProperty:
    //   role::tab     TextButton drawn as a header tab (underline when selected).
    //                 Optional "ahe.badge" property draws a small pill, e.g. "PHASE 5".
    //   role::bypass  ToggleButton drawn as a status switch. Uses properties
    //                 "ahe.onText" / "ahe.offText" for the label in each state and
    //                 "ahe.onMeansActive" (bool) to say whether ON = processing.
    // -----------------------------------------------------------------------
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        // Knobs
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

        // Buttons, tabs, switches
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool isHighlighted, bool isDown) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isHighlighted, bool isDown) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        // Any text drawn with JUCE's default font (typed values, tooltips, menus)
        // uses the embedded Inter instead of the system font.
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

        // Labels, tooltips, resizer
        juce::Font getLabelFont (juce::Label&) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
        void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

        // Focus rings are only drawn while the user is navigating with the
        // keyboard ("focus-visible"); a mouse click hides them again. See
        // KeyboardFocusIndicator below, which drives this flag.
        void setKeyboardNavigationActive (bool active) noexcept { keyboardNavigationActive = active; }
        bool isKeyboardNavigationActive() const noexcept         { return keyboardNavigationActive; }
        bool shouldShowFocus (const juce::Component& c) const;

        // Shared helpers for custom components
        static void drawPanel (juce::Graphics&, juce::Rectangle<float> area, float cornerRadius,
                               juce::Colour fill = theme::colour::panel,
                               juce::Colour outline = theme::colour::border);
        static void drawFocusRing (juce::Graphics&, juce::Rectangle<float> area, float cornerRadius);

    private:
        bool keyboardNavigationActive = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
    };

    // -----------------------------------------------------------------------
    // KeyboardFocusIndicator — attach one to a plug-in editor. Tab / Shift+Tab
    // turns focus rings on; any mouse click turns them off. Keys are never
    // consumed, so controls and the host still receive them.
    // -----------------------------------------------------------------------
    class KeyboardFocusIndicator : private juce::KeyListener,
                                   private juce::MouseListener
    {
    public:
        KeyboardFocusIndicator (juce::Component& root, LookAndFeel& lookAndFeel);
        ~KeyboardFocusIndicator() override;

    private:
        bool keyPressed (const juce::KeyPress&, juce::Component*) override;
        void mouseDown (const juce::MouseEvent&) override;
        void set (bool active);

        juce::Component& root;
        LookAndFeel& lnf;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardFocusIndicator)
    };
}
