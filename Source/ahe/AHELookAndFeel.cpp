#include "AHELookAndFeel.h"

namespace ahe
{
    using namespace theme;

    namespace
    {
        bool hasRole (const juce::Component& c, const juce::String& role)
        {
            return c.getProperties()[roleProperty].toString() == role;
        }

        juce::Path makeArc (juce::Point<float> centre, float radius, float from, float to)
        {
            juce::Path p;
            p.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, from, to, true);
            return p;
        }
    }

    LookAndFeel::LookAndFeel()
    {
        using C = juce::Colour;
        auto set = [this] (int id, C c) { setColour (id, c); };

        set (juce::ResizableWindow::backgroundColourId, colour::canvas);

        set (juce::Label::textColourId, colour::textPrimary);
        set (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        set (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        set (juce::Label::textWhenEditingColourId, colour::textPrimary);
        set (juce::Label::backgroundWhenEditingColourId, colour::raised);
        set (juce::Label::outlineWhenEditingColourId, colour::purpleSoft);

        set (juce::Slider::textBoxTextColourId, colour::textPrimary);
        set (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        set (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        set (juce::Slider::textBoxHighlightColourId, colour::brandPurple.withAlpha (0.5f));
        set (juce::Slider::rotarySliderFillColourId, colour::brandPurple);
        set (juce::Slider::rotarySliderOutlineColourId, colour::border);
        set (juce::Slider::thumbColourId, colour::textPrimary);

        set (juce::TextEditor::textColourId, colour::textPrimary);
        set (juce::TextEditor::backgroundColourId, colour::raised);
        set (juce::TextEditor::highlightColourId, colour::brandPurple.withAlpha (0.5f));
        set (juce::TextEditor::focusedOutlineColourId, colour::purpleSoft);
        set (juce::CaretComponent::caretColourId, colour::textPrimary);

        set (juce::TextButton::buttonColourId, colour::raised);
        set (juce::TextButton::buttonOnColourId, colour::brandPurple);
        set (juce::TextButton::textColourOffId, colour::textSecondary);
        set (juce::TextButton::textColourOnId, colour::textPrimary);
        set (juce::ToggleButton::textColourId, colour::textPrimary);

        set (juce::TooltipWindow::backgroundColourId, colour::raised);
        set (juce::TooltipWindow::textColourId, colour::textPrimary);
        set (juce::TooltipWindow::outlineColourId, colour::border);
    }

    // ---------------------------------------------------------------- knobs

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float startAngle, float endAngle, juce::Slider& slider)
    {
        const bool enabled = slider.isEnabled();
        const bool active  = enabled && slider.isMouseOverOrDragging();
        const bool focused = enabled && shouldShowFocus (slider);

        auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  centre = bounds.getCentre();
        const float radius = size * 0.5f;
        const float trackW = juce::jlimit (3.0f, 7.0f, radius * 0.12f);
        const float arcR   = radius - trackW * 0.5f;

        // Track (full range)
        g.setColour (colour::border);
        g.strokePath (makeArc (centre, arcR, startAngle, endAngle),
                      juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc. Bipolar ranges (e.g. -24..+24 dB) fill from the centre/default
        // so "0 dB = untouched" reads at a glance; unipolar ranges fill from the start.
        const double min = slider.getMinimum(), max = slider.getMaximum();
        const bool bipolar = min < 0.0 && max > 0.0;
        const float origin = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
        const float fromA  = startAngle + origin * (endAngle - startAngle);
        const float toA    = startAngle + pos    * (endAngle - startAngle);

        if (std::abs (toA - fromA) > 0.001f)
        {
            auto valueColour = active ? colour::purpleSoft : colour::brandPurple;
            if (! enabled) valueColour = colour::textMuted;

            g.setColour (valueColour);
            g.strokePath (makeArc (centre, arcR, juce::jmin (fromA, toA), juce::jmax (fromA, toA)),
                          juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Centre tick for bipolar knobs (marks the default / unity point)
        if (bipolar)
        {
            const auto tickOuter = centre.getPointOnCircumference (radius + 1.5f, fromA);
            const auto tickInner = centre.getPointOnCircumference (radius - trackW - 2.5f, fromA);
            g.setColour (colour::textMuted);
            g.drawLine ({ tickInner, tickOuter }, 1.5f);
        }

        // Body
        const float bodyR = radius - trackW - juce::jmax (4.0f, radius * 0.1f);
        auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
        g.setColour (active ? colour::raised.brighter (0.08f) : colour::raised);
        g.fillEllipse (body);
        g.setColour (active ? colour::purpleSoft.withAlpha (0.6f) : colour::border);
        g.drawEllipse (body, 1.2f);

        // Pointer
        const auto p1 = centre.getPointOnCircumference (bodyR * 0.30f, toA);
        const auto p2 = centre.getPointOnCircumference (bodyR * 0.86f, toA);
        g.setColour (enabled ? colour::textPrimary : colour::textMuted);
        g.drawLine ({ p1, p2 }, juce::jmax (2.0f, trackW * 0.55f));

        // Keyboard focus: dashed ring outside the arc (shape, not just colour)
        if (focused)
        {
            auto ring = juce::Rectangle<float> (size + 2.0f, size + 2.0f).withCentre (centre);
            juce::Path circle;
            circle.addEllipse (ring);
            juce::Path dashed;
            const float dashes[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, circle, dashes, 2);
            g.setColour (colour::purpleSoft);
            g.fillPath (dashed);
        }
    }

    juce::Slider::SliderLayout LookAndFeel::getSliderLayout (juce::Slider& slider)
    {
        auto layout = LookAndFeel_V4::getSliderLayout (slider);

        // Give the value read-out a little breathing room below the knob.
        if (slider.getTextBoxPosition() == juce::Slider::TextBoxBelow)
            layout.sliderBounds = layout.sliderBounds.withTrimmedBottom (2);

        return layout;
    }

    juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (slider);
        const bool compact = (bool) slider.getProperties().getWithDefault ("ahe.compact", false);
        l->setFont (juce::Font (font::value (compact ? 11.5f : 13.0f)));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, colour::textPrimary);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        return l;
    }

    // -------------------------------------------------------------- buttons

    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool isHighlighted, bool isDown)
    {
        auto area = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool enabled = b.isEnabled();

        if (hasRole (b, role::tab))
        {
            if (b.getToggleState())
            {
                // Selected tab: soft fill + solid purple underline
                g.setColour (colour::brandPurple.withAlpha (0.14f));
                g.fillRoundedRectangle (area, radius::small);
                g.setColour (colour::brandPurple);
                g.fillRoundedRectangle (area.removeFromBottom (2.5f).reduced (8.0f, 0.0f), 1.25f);
            }
            else if (enabled && (isHighlighted || isDown))
            {
                g.setColour (colour::raised);
                g.fillRoundedRectangle (area, radius::small);
            }
        }
        else if (hasRole (b, role::primary))
        {
            // Main action: solid brand purple, lighter on hover / press.
            auto fill = colour::brandPurple;
            if (enabled && isDown)             fill = fill.brighter (0.25f);
            else if (enabled && isHighlighted) fill = fill.brighter (0.12f);
            drawPanel (g, area, radius::card, enabled ? fill : colour::panel,
                       enabled ? colour::purpleSoft : colour::border);
        }
        else
        {
            auto fill = colour::raised;
            if (enabled && isDown)             fill = fill.brighter (0.12f);
            else if (enabled && isHighlighted) fill = fill.brighter (0.06f);
            drawPanel (g, area, radius::small, enabled ? fill : colour::panel,
                       enabled && isHighlighted ? colour::purpleSoft.withAlpha (0.6f) : colour::border);
        }

        if (enabled && shouldShowFocus (b))
            drawFocusRing (g, b.getLocalBounds().toFloat(), radius::small);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& b, int)
    {
        if (hasRole (b, role::primary))
            return juce::Font (font::display (30.0f));

        return hasRole (b, role::tab) ? juce::Font (font::label (11.5f)) : juce::Font (font::value (13.0f));
    }

    void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
    {
        const bool enabled = b.isEnabled();
        auto area = b.getLocalBounds().reduced (6, 0);

        auto textColour = (b.getToggleState() || hasRole (b, role::primary)) ? colour::textPrimary : colour::textSecondary;
        if (! enabled) textColour = colour::textMuted;

        const auto font  = getTextButtonFont (b, b.getHeight());
        const auto badge = b.getProperties()["ahe.badge"].toString();

        if (badge.isEmpty())
        {
            g.setColour (textColour);
            g.setFont (font);
            g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1);
            return;
        }

        // Text + small outlined pill badge (e.g. "PHASE 5"), centred as a group.
        const juce::Font badgeFont (font::label (8.5f));
        const int textW  = juce::GlyphArrangement::getStringWidthInt (font, b.getButtonText());
        const int badgeW = juce::GlyphArrangement::getStringWidthInt (badgeFont, badge) + 10;
        const int gap = 6;
        auto group = area.withSizeKeepingCentre (textW + gap + badgeW, area.getHeight());

        g.setColour (textColour);
        g.setFont (font);
        g.drawText (b.getButtonText(), group.removeFromLeft (textW), juce::Justification::centred, false);
        group.removeFromLeft (gap);

        auto pill = group.withSizeKeepingCentre (badgeW, 15).toFloat();
        g.setColour (colour::textMuted);
        g.drawRoundedRectangle (pill.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (colour::textSecondary);
        g.setFont (badgeFont);
        g.drawText (badge, pill.toNearestInt(), juce::Justification::centred, false);
    }

    void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool isHighlighted, bool isDown)
    {
        if (! hasRole (b, role::bypass))
        {
            LookAndFeel_V4::drawToggleButton (g, b, isHighlighted, isDown);
            return;
        }

        const auto& props = b.getProperties();
        const bool on = b.getToggleState();
        const bool onMeansActive = (bool) props.getWithDefault ("ahe.onMeansActive", true);
        const bool active = (on == onMeansActive);
        const bool enabled = b.isEnabled();

        auto area = b.getLocalBounds().toFloat().reduced (1.0f);
        const float r = area.getHeight() * 0.5f;

        auto fill = active ? colour::alienGreen.withAlpha (0.12f) : colour::raised;
        if (enabled && (isHighlighted || isDown)) fill = fill.brighter (0.08f);
        drawPanel (g, area, r, fill, active ? colour::alienGreen.withAlpha (0.7f) : colour::border);

        // Indicator: filled dot = processing, hollow ring with slash = bypassed.
        auto dot = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ area.getX() + r + 2.0f, area.getCentreY() });
        if (active)
        {
            g.setColour (colour::alienGreen);
            g.fillEllipse (dot);
        }
        else
        {
            g.setColour (colour::textSecondary);
            g.drawEllipse (dot, 1.5f);
            g.drawLine ({ dot.getBottomLeft().translated (1.5f, -1.5f), dot.getTopRight().translated (-1.5f, 1.5f) }, 1.5f);
        }

        const auto text = (on ? props["ahe.onText"] : props["ahe.offText"]).toString();
        g.setColour (! enabled ? colour::textMuted : active ? colour::textPrimary : colour::textSecondary);
        g.setFont (juce::Font (font::label (11.5f)));
        g.drawText (text, area.withTrimmedLeft (r * 2.0f + 6.0f).withTrimmedRight (r * 0.6f).toNearestInt(),
                    juce::Justification::centredLeft, false);

        if (enabled && shouldShowFocus (b))
            drawFocusRing (g, b.getLocalBounds().toFloat(), r);
    }

    // ------------------------------------------------- labels and tooltips

    juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
    {
        if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
            return f.isBold() ? fonts::uiSemiBold() : fonts::uiRegular();

        return LookAndFeel_V4::getTypefaceForFont (f);
    }

    juce::Font LookAndFeel::getLabelFont (juce::Label& l)
    {
        return l.getFont();
    }

    juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
    {
        const juce::Font f (font::body (12.5f));
        juce::AttributedString s;
        s.append (tipText, f, colour::textPrimary);
        juce::TextLayout layout;
        layout.createLayout (s, 260.0f);

        const int w = (int) std::ceil (layout.getWidth()) + 20;
        const int h = (int) std::ceil (layout.getHeight()) + 14;

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                     w, h).constrainedWithin (parentArea);
    }

    void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        auto area = juce::Rectangle<int> (width, height).toFloat();
        drawPanel (g, area.reduced (0.5f), 8.0f, colour::raised, colour::border);

        juce::AttributedString s;
        s.append (text, juce::Font (font::body (12.5f)), colour::textPrimary);
        juce::TextLayout layout;
        layout.createLayout (s, 260.0f);
        layout.draw (g, area.reduced (10.0f, 7.0f));
    }

    void LookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
    {
        g.setColour ((isMouseOver || isMouseDragging) ? colour::purpleSoft : colour::textMuted);
        for (int i = 0; i < 3; ++i)
        {
            const float d = 4.0f + (float) i * 4.0f;
            g.drawLine ((float) w - d, (float) h - 3.0f, (float) w - 3.0f, (float) h - d, 1.2f);
        }
    }

    // ------------------------------------------------------------- focus

    bool LookAndFeel::shouldShowFocus (const juce::Component& c) const
    {
        return keyboardNavigationActive && c.hasKeyboardFocus (true);
    }

    KeyboardFocusIndicator::KeyboardFocusIndicator (juce::Component& r, LookAndFeel& l)
        : root (r), lnf (l)
    {
        root.addKeyListener (this);
        root.addMouseListener (this, true);
    }

    KeyboardFocusIndicator::~KeyboardFocusIndicator()
    {
        root.removeKeyListener (this);
        root.removeMouseListener (this);
    }

    bool KeyboardFocusIndicator::keyPressed (const juce::KeyPress& key, juce::Component*)
    {
        if (key.getKeyCode() == juce::KeyPress::tabKey)
            set (true);
        return false;   // never consume: let JUCE move focus / let the host see the key
    }

    void KeyboardFocusIndicator::mouseDown (const juce::MouseEvent&)
    {
        set (false);
    }

    void KeyboardFocusIndicator::set (bool active)
    {
        if (lnf.isKeyboardNavigationActive() != active)
        {
            lnf.setKeyboardNavigationActive (active);
            root.repaint();
        }
    }

    // ------------------------------------------------------------- helpers

    void LookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius,
                                 juce::Colour fill, juce::Colour outline)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (area, cornerRadius);
        g.setColour (outline);
        g.drawRoundedRectangle (area.reduced (0.5f), cornerRadius, 1.0f);
    }

    void LookAndFeel::drawFocusRing (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius)
    {
        juce::Path outline;
        outline.addRoundedRectangle (area.reduced (0.75f), cornerRadius);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (colour::purpleSoft);
        g.fillPath (dashed);
    }
}
