#pragma once

#include "AHETheme.h"

namespace ahe
{
    // -----------------------------------------------------------------------
    // Header — the shared AHE Audio plug-in header bar.
    //
    //   [alien mark] PRODUCT NAME      ...centre area...      [AHE AUDIO lockup]
    //
    // The centre area is left free for the plug-in's own navigation (tabs);
    // use getCentreArea() from the parent's resized() to place them.
    // -----------------------------------------------------------------------
    class Header : public juce::Component
    {
    public:
        explicit Header (juce::String productName);

        void paint (juce::Graphics&) override;
        void resized() override;

        // Free space between the product name and the maker mark (header coords).
        juce::Rectangle<int> getCentreArea() const noexcept { return centreArea; }

    private:
        juce::String productName;
        std::unique_ptr<juce::Drawable> alienMark, makerMark;
        juce::Rectangle<int> markArea, nameArea, makerArea, centreArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Header)
    };
}
