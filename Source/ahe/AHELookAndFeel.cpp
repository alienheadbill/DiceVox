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

        set (juce::ComboBox::backgroundColourId, colour::raised);
        set (juce::ComboBox::textColourId, colour::textPrimary);
        set (juce::ComboBox::outlineColourId, colour::border);
        set (juce::ComboBox::arrowColourId, colour::textSecondary);
        set (juce::ComboBox::focusedOutlineColourId, colour::purpleSoft);
        set (juce::PopupMenu::backgroundColourId, colour::raised);
        set (juce::PopupMenu::textColourId, colour::textPrimary);
        set (juce::PopupMenu::highlightedBackgroundColourId, colour::brandPurple.withAlpha (0.55f));
        set (juce::PopupMenu::highlightedTextColourId, colour::textPrimary);
    }

    // ---------------------------------------------------------------- knobs

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float startAngle, float endAngle, juce::Slider& slider)
    {
        // "Scanner" knob: an outer ring of ticks that light up to the value, a
        // thin glowing value arc, a dark lens-like body and a lit pointer.
        const bool enabled = slider.isEnabled();
        const bool active  = enabled && slider.isMouseOverOrDragging();
        const bool focused = enabled && shouldShowFocus (slider);

        auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  centre = bounds.getCentre();
        const float radius = size * 0.5f;
        const float trackW = juce::jlimit (2.0f, 4.0f, radius * 0.08f);
        const float arcR   = radius - juce::jmax (4.0f, radius * 0.17f);

        // Value range. Bipolar ranges (e.g. -24..+24 dB) fill from the centre/default
        // so "0 dB = untouched" reads at a glance; unipolar ranges fill from the start.
        const double min = slider.getMinimum(), max = slider.getMaximum();
        const bool bipolar = min < 0.0 && max > 0.0;
        const float origin = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
        const float fromA  = startAngle + origin * (endAngle - startAngle);
        const float toA    = startAngle + pos    * (endAngle - startAngle);

        // Tick ring: lit between the origin and the value (every fifth tick longer).
        {
            const int numTicks = radius > 28.0f ? 31 : 21;
            const float lo = juce::jmin (origin, pos), hi = juce::jmax (origin, pos);
            for (int i = 0; i < numTicks; ++i)
            {
                const float t = (float) i / (float) (numTicks - 1);
                const float a = startAngle + t * (endAngle - startAngle);
                const bool lit = enabled && t >= lo - 1.0e-4f && t <= hi + 1.0e-4f && hi - lo > 1.0e-4f;
                const float inner = radius - ((i % 5 == 0) ? juce::jmax (3.5f, radius * 0.12f) : juce::jmax (2.0f, radius * 0.07f));
                g.setColour (lit ? colour::purpleSoft.withAlpha (active ? 1.0f : 0.85f) : colour::border.brighter (0.1f));
                g.drawLine ({ centre.getPointOnCircumference (inner, a), centre.getPointOnCircumference (radius, a) }, 1.2f);
            }
        }

        // Track (full range), hairline
        g.setColour (colour::border);
        g.strokePath (makeArc (centre, arcR, startAngle, endAngle),
                      juce::PathStrokeType (trackW * 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc: a soft glow under a crisp core.
        if (std::abs (toA - fromA) > 0.001f)
        {
            auto valueColour = active ? colour::purpleSoft : colour::brandPurple.brighter (0.15f);
            if (! enabled) valueColour = colour::textMuted;

            const auto arc = makeArc (centre, arcR, juce::jmin (fromA, toA), juce::jmax (fromA, toA));
            if (enabled)
            {
                g.setColour (colour::brandPurple.withAlpha (active ? 0.35f : 0.22f));
                g.strokePath (arc, juce::PathStrokeType (trackW * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (valueColour);
            g.strokePath (arc, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Body: a dark lens, lit from the top left, with a faint inner ring.
        const float bodyR = arcR - trackW - juce::jmax (3.0f, radius * 0.09f);
        auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
        juce::ColourGradient lens (colour::raised.brighter (active ? 0.22f : 0.12f), body.getX() + bodyR * 0.5f, body.getY() + bodyR * 0.3f,
                                   colour::canvas, body.getRight(), body.getBottom(), true);
        g.setGradientFill (lens);
        g.fillEllipse (body);
        g.setColour (active ? colour::purpleSoft.withAlpha (0.7f) : colour::border.brighter (0.15f));
        g.drawEllipse (body, 1.0f);
        g.setColour (colour::purpleSoft.withAlpha (0.10f));
        g.drawEllipse (body.reduced (bodyR * 0.22f), 1.0f);

        // Pointer: a line on the body, and a lit dot riding the value arc.
        const auto p1 = centre.getPointOnCircumference (bodyR * 0.30f, toA);
        const auto p2 = centre.getPointOnCircumference (bodyR * 0.82f, toA);
        g.setColour (enabled ? colour::textPrimary : colour::textMuted);
        g.drawLine ({ p1, p2 }, juce::jmax (1.6f, trackW * 0.6f));

        if (enabled)
        {
            const auto tip = centre.getPointOnCircumference (arcR, toA);
            g.setColour (colour::purpleSoft.withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (trackW * 3.6f, trackW * 3.6f).withCentre (tip));
            g.setColour (colour::textPrimary);
            g.fillEllipse (juce::Rectangle<float> (trackW * 1.5f, trackW * 1.5f).withCentre (tip));
        }

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
                // Selected tab: faint glass + a glowing underline
                g.setColour (colour::brandPurple.withAlpha (0.12f));
                g.fillRoundedRectangle (area, radius::small);
                auto line = area.removeFromBottom (2.0f).reduced (10.0f, 0.0f);
                g.setColour (colour::brandPurple.withAlpha (0.35f));
                g.fillRoundedRectangle (line.expanded (0.0f, 2.0f).translated (0.0f, -1.0f), 2.0f);
                g.setColour (colour::purpleSoft);
                g.fillRoundedRectangle (line, 1.0f);
            }
            else if (enabled && (isHighlighted || isDown))
            {
                g.setColour (colour::raised.withAlpha (0.7f));
                g.fillRoundedRectangle (area, radius::small);
            }
        }
        else if (hasRole (b, role::lock) || hasRole (b, role::segment))
        {
            // Small glass chips; the selected / locked one is lit purple.
            const bool on = b.getToggleState();
            if (on)
            {
                juce::ColourGradient lit (colour::brandPurple.withAlpha (0.55f), area.getX(), area.getY(),
                                          colour::brandPurple.withAlpha (0.25f), area.getX(), area.getBottom(), false);
                g.setGradientFill (lit);
                g.fillRoundedRectangle (area, radius::small * 0.7f);
                g.setColour (colour::purpleSoft);
                g.drawRoundedRectangle (area.reduced (0.5f), radius::small * 0.7f, 1.0f);
            }
            else
            {
                drawGlass (g, area, radius::small * 0.7f,
                           enabled && (isHighlighted || isDown) ? colour::purpleSoft.withAlpha (0.5f) : colour::border, false);
            }
        }
        else if (hasRole (b, role::primary))
        {
            // Main action: an energy-cell button, purple gradient with a bright rim
            // and a highlight along the top.
            const float r = radius::card;
            auto top = colour::brandPurple.brighter (0.25f), bottom = colour::brandPurple.darker (0.45f);
            if (enabled && isDown)             { top = top.darker (0.15f); bottom = bottom.darker (0.15f); }
            else if (enabled && isHighlighted) { top = top.brighter (0.15f); bottom = bottom.brighter (0.15f); }
            if (! enabled) top = bottom = colour::panel;

            juce::ColourGradient cell (top, area.getX(), area.getY(), bottom, area.getX(), area.getBottom(), false);
            g.setGradientFill (cell);
            g.fillRoundedRectangle (area, r);
            g.setColour (juce::Colours::white.withAlpha (enabled ? 0.22f : 0.05f));
            g.fillRoundedRectangle (area.getX() + r, area.getY() + 2.0f, area.getWidth() - 2.0f * r, 1.5f, 0.75f);
            g.setColour (enabled ? colour::purpleSoft : colour::border);
            g.drawRoundedRectangle (area.reduced (0.5f), r, 1.2f);
        }
        else
        {
            auto outline = colour::border;
            if (enabled && (isHighlighted || isDown)) outline = colour::purpleSoft.withAlpha (isDown ? 0.9f : 0.6f);
            drawGlass (g, area, radius::small, outline, false);
        }

        if (enabled && shouldShowFocus (b))
            drawFocusRing (g, b.getLocalBounds().toFloat(), radius::small);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& b, int)
    {
        if (hasRole (b, role::primary))
            return juce::Font (font::display (30.0f));
        if (hasRole (b, role::segment))
            return juce::Font (font::label (10.0f));

        return hasRole (b, role::tab) ? juce::Font (font::label (11.5f)) : juce::Font (font::value (13.0f));
    }

    void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
    {
        const bool enabled = b.isEnabled();
        auto area = b.getLocalBounds().reduced (6, 0);

        auto textColour = (b.getToggleState() || hasRole (b, role::primary)) ? colour::textPrimary : colour::textSecondary;
        if (! enabled) textColour = colour::textMuted;

        if (hasRole (b, role::lock))
        {
            // Padlock: body + shackle. Locked = closed shackle, unlocked = open (lifted).
            const bool on = b.getToggleState();
            auto icon = b.getLocalBounds().toFloat().withSizeKeepingCentre (11.0f, 13.0f);
            auto body = icon.removeFromBottom (7.5f);
            const float r = 3.6f;
            const float cx = body.getCentreX() + (on ? 0.0f : 2.0f);
            const float top = body.getY() - (on ? 0.0f : 2.5f);
            juce::Path shackle;
            shackle.startNewSubPath (cx - r, on ? body.getY() : top + 1.0f); // open: left leg free
            shackle.lineTo (cx - r, top);
            shackle.addCentredArc (cx, top, r, r, 0.0f,
                                   -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
            shackle.lineTo (cx + r, body.getY());

            g.setColour (! enabled ? colour::textMuted : on ? colour::textPrimary : colour::textSecondary);
            g.strokePath (shackle, juce::PathStrokeType (1.6f));
            g.fillRoundedRectangle (body, 1.5f);
            return;
        }

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

        // Glass pill; when processing, a green hairline and a glowing LED.
        if (active)
        {
            g.setColour (colour::alienGreen.withAlpha (enabled && (isHighlighted || isDown) ? 0.18f : 0.10f));
            g.fillRoundedRectangle (area, r);
            g.setColour (colour::alienGreen.withAlpha (0.75f));
            g.drawRoundedRectangle (area.reduced (0.5f), r, 1.0f);
        }
        else
        {
            drawGlass (g, area, r, enabled && (isHighlighted || isDown) ? colour::purpleSoft.withAlpha (0.5f) : colour::border, false);
        }

        // Indicator: glowing dot = processing, hollow ring with slash = bypassed / off.
        auto dot = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ area.getX() + r + 2.0f, area.getCentreY() });
        if (active)
        {
            g.setColour (colour::alienGreen.withAlpha (0.30f));
            g.fillEllipse (dot.expanded (3.5f));
            g.setColour (colour::greenSoft);
            g.fillEllipse (dot);
            g.setColour (colour::alienGreen);
            g.drawEllipse (dot, 1.0f);
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
        drawPanel (g, area.reduced (0.5f), 8.0f, colour::raised, colour::purpleSoft.withAlpha (0.45f));

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

    void LookAndFeel::drawGlass (juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius,
                                 juce::Colour outline, bool brackets)
    {
        // Frosted glass: a slightly lighter top fading to the panel colour, so
        // the starfield behind still shows faintly.
        juce::ColourGradient fill (colour::raised.withAlpha (0.66f), area.getX(), area.getY(),
                                   colour::panel.withAlpha (0.82f), area.getX(), area.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (area, cornerRadius);

        // A faint sheen along the top edge, then the hairline outline.
        if (area.getWidth() > 2.0f * cornerRadius + 4.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (area.getX() + cornerRadius, area.getY() + 1.0f, area.getWidth() - 2.0f * cornerRadius, 1.0f);
        }
        g.setColour (outline);
        g.drawRoundedRectangle (area.reduced (0.5f), cornerRadius, 1.0f);

        // HUD corner brackets just inside the rim.
        if (brackets)
        {
            const float len = juce::jmin (12.0f, area.getWidth() * 0.12f, area.getHeight() * 0.2f);
            const float in = juce::jmax (4.0f, cornerRadius * 0.45f);
            auto inner = area.reduced (in);
            juce::Path p;
            p.startNewSubPath (inner.getX(), inner.getY() + len);       p.lineTo (inner.getX(), inner.getY());       p.lineTo (inner.getX() + len, inner.getY());
            p.startNewSubPath (inner.getRight() - len, inner.getY());   p.lineTo (inner.getRight(), inner.getY());   p.lineTo (inner.getRight(), inner.getY() + len);
            p.startNewSubPath (inner.getRight(), inner.getBottom() - len); p.lineTo (inner.getRight(), inner.getBottom()); p.lineTo (inner.getRight() - len, inner.getBottom());
            p.startNewSubPath (inner.getX() + len, inner.getBottom());  p.lineTo (inner.getX(), inner.getBottom());  p.lineTo (inner.getX(), inner.getBottom() - len);
            g.setColour (colour::purpleSoft.withAlpha (0.45f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int, int, int, int, juce::ComboBox& box)
    {
        auto area = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
        const bool hot = box.isEnabled() && (box.isMouseOver (true) || isButtonDown);
        drawGlass (g, area, radius::small, hot ? colour::purpleSoft.withAlpha (0.6f) : colour::border, false);

        // Chevron
        const float cx = (float) width - 15.0f, cy = (float) height * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (hot ? colour::purpleSoft : colour::textSecondary);
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (box.isEnabled() && shouldShowFocus (box))
            drawFocusRing (g, area, radius::small);
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
