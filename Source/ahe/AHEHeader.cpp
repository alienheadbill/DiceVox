#include "AHEHeader.h"
#include "AHEBrandMarks.h"

namespace ahe
{
    using namespace theme;

    Header::Header (juce::String name)
        : productName (std::move (name)),
          alienMark (brand::alienIcon()),
          makerMark (brand::lockup())
    {
        setTitle ("AHE Audio " + productName);
        setDescription ("Plug-in header: " + productName + " by AHE Audio, a division of Alien Head Entertainment");
        setInterceptsMouseClicks (false, true);
    }

    void Header::resized()
    {
        auto area = getLocalBounds().reduced (space::l, 0);

        markArea = area.removeFromLeft (getHeight() - space::m).withSizeKeepingCentre (getHeight() - space::m,
                                                                                       getHeight() - space::m);
        area.removeFromLeft (space::s);

        const juce::Font nameFont (font::display (22.0f));
        const int nameW = juce::GlyphArrangement::getStringWidthInt (nameFont, productName.toUpperCase()) + 6;
        nameArea = area.removeFromLeft (nameW);

        // Lockup SVG is 476x186; keep its aspect ratio.
        const int makerH = getHeight() - space::m;
        makerArea = area.removeFromRight (juce::roundToInt ((float) makerH * 476.0f / 186.0f))
                        .withSizeKeepingCentre (juce::roundToInt ((float) makerH * 476.0f / 186.0f), makerH);

        centreArea = area.reduced (space::m, 0);
    }

    void Header::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (colour::panel);
        g.fillRect (bounds);
        g.setColour (colour::border);
        g.fillRect (bounds.removeFromBottom (1.0f));

        if (alienMark != nullptr)
            alienMark->drawWithin (g, markArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);

        g.setColour (colour::textPrimary);
        g.setFont (juce::Font (font::display (22.0f)));
        g.drawText (productName.toUpperCase(), nameArea, juce::Justification::centredLeft, false);

        if (makerMark != nullptr)
            makerMark->drawWithin (g, makerArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);
    }
}
