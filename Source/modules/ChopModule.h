#pragma once

#include "EffectModule.h"

// ---------------------------------------------------------------------------
// ChopModule
//
// Tempo-synced stutter gate: cuts the vocal on and off in a rhythm (1/32 to
// 1/4 notes, straight or triplet). Depth sets how far the gaps drop (100 % =
// silence). While the host plays, the chops lock to the song's beat grid;
// otherwise they free-run at the host tempo (120 BPM if none). The gate edges
// are rounded off over about 2 ms, so the chops never click.
// ---------------------------------------------------------------------------
class ChopModule : public EffectModule
{
public:
    static constexpr auto rateId  = "chop_rate";
    static constexpr auto depthId = "chop_depth";

    static const juce::StringArray& rateNames();
    static double rateBeats (int index);
    static constexpr int defaultRate = 1; // 1/16

    juce::String getId() const override { return "chop"; }
    juce::String getDisplayName() const override { return "Chop"; }
    int getParameterVersion() const override { return 2; }

    void addParameters (ParamList& params) const override;
    void attach (juce::AudioProcessorValueTreeState& state) override;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) override;

    void setHostTempo (double bpm) override;
    void setHostPosition (double ppq) override;

private:
    std::atomic<float>* rateChoice = nullptr;
    std::atomic<float>* depthPct   = nullptr;

    double sampleRate = 48000.0, tempo = 120.0;
    double beats = 0.0;          // position in quarter notes (free-running or host-locked)
    double pendingPpq = -1.0;    // host position for the next block, or < 0
    float gain = 1.0f, edgeCoeff = 1.0f;
};
