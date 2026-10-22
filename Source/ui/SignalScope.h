#pragma once

#include "ahe/AHETheme.h"
#include <vector>

class DiceVoxProcessor;

// ---------------------------------------------------------------------------
// SignalScope — a live read-out of the vocal leaving DiceVox.
//
//   SIGNAL  ~~~~~~~~^^^^~~~~~~^^^~~~~~   (newest on the right)
//
// A mirrored peak waveform with a soft glow, redrawn 30 times a second from
// the processor's lock-free scope buffer. Flat when nothing plays. Purely a
// display: it never touches the audio.
// ---------------------------------------------------------------------------
class SignalScope : public juce::Component,
                    private juce::Timer
{
public:
    explicit SignalScope (DiceVoxProcessor&);

    void paint (juce::Graphics&) override;

    // Pull the latest points now (the timer does this 30 times a second).
    void refreshNow();

private:
    void timerCallback() override;

    DiceVoxProcessor& processor;
    std::vector<float> points;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalScope)
};
