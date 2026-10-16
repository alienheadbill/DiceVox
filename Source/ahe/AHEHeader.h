#pragma once

#include "AHETheme.h"

namespace ahe
{
    // -----------------------------------------------------------------------
    // Header — the shared AHE Audio plug-in header bar.
    //
    //   [alien mark] PRODUCT NAME      ...centre area...      [AHE] AUDIO
    //
    // The alien appears once (left, the primary mark). The maker mark on the
    // right is the compact one-line form: the real AHE letter artwork from the
    // approved lockup plus "AUDIO" in Inter, legible at plug-in header size.
    // The full stacked lockup is reserved for installers, splash and marketing.
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
        std::unique_ptr<juce::Drawable> alienMark, wordmark;
        juce::Rectangle<int> markArea, nameArea, makerArea, centreArea;

        static constexpr float wordmarkHeight = 17.0f;
        static constexpr float audioTextSize  = 11.5f;
        static constexpr float makerGap       = 7.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Header)
    };
}
