#include "AHEHeader.h"
#include "AHEBrandMarks.h"

namespace ahe
{
    using namespace theme;

    Header::Header (juce::String name)
        : productName (std::move (name)),
          alienMark (brand::alienIcon()),
          wordmark (brand::wordmark())
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

        const juce::Font nameFont (font::display (27.0f));
        const int nameW = juce::GlyphArrangement::getStringWidthInt (nameFont, productName.toUpperCase()) + 6;
        nameArea = area.removeFromLeft (nameW);

        // Compact maker mark: AHE letters (aspect from the artwork) + "AUDIO".
        float wordmarkW = 0.0f;
        if (wordmark != nullptr)
        {
            const auto b = wordmark->getDrawableBounds();
            wordmarkW = b.getHeight() > 0.0f ? wordmarkHeight * b.getWidth() / b.getHeight() : 0.0f;
        }
        const juce::Font audioFont (font::label (audioTextSize).withKerningFactor (0.22f));
        const float audioW = (float) juce::GlyphArrangement::getStringWidth (audioFont, "AUDIO");
        const int makerW = (int) std::ceil (wordmarkW + makerGap + audioW);
        makerArea = area.removeFromRight (makerW).withSizeKeepingCentre (makerW, (int) wordmarkHeight + 2);

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
        g.setFont (juce::Font (font::display (27.0f)));
        g.drawText (productName.toUpperCase(), nameArea, juce::Justification::centredLeft, false);

        // Maker mark: [AHE] AUDIO
        auto maker = makerArea.toFloat();
        if (wordmark != nullptr)
        {
            const auto b = wordmark->getDrawableBounds();
            const float w = wordmarkHeight * b.getWidth() / b.getHeight();
            wordmark->drawWithin (g, maker.removeFromLeft (w).withSizeKeepingCentre (w, wordmarkHeight),
                                  juce::RectanglePlacement::centred, 1.0f);
            maker.removeFromLeft (makerGap);
        }
        g.setColour (colour::textPrimary);
        g.setFont (juce::Font (font::label (audioTextSize).withKerningFactor (0.22f)));
        g.drawText ("AUDIO", maker.toNearestInt(), juce::Justification::centredLeft, false);
    }
}
