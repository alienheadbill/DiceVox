// Offline tests: run the plugin's audio engine without a DAW.
// Build target "DiceVoxTests" and run it; exit code 0 = all passed.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"

#include <cmath>
#include <iostream>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize  = 512;
    constexpr int    numBlocks  = 40;

    int failures = 0;

    void check (bool ok, const std::string& name, const std::string& detail = {})
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << name;
        if (! detail.empty()) std::cout << "  (" << detail << ")";
        std::cout << "\n";
        if (! ok) ++failures;
    }

    void setParam (DiceVoxProcessor& p, const juce::String& id, float realValue)
    {
        auto* param = p.state.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }

    // Stereo test signal: vocal-range sine on L, noise on R.
    juce::AudioBuffer<float> makeSignal (int numSamples, int startSample)
    {
        juce::AudioBuffer<float> buf (2, numSamples);
        juce::Random rng (1234 + startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (startSample + i) / sampleRate;
            buf.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t));
            buf.setSample (1, i, 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
        }
        return buf;
    }

    struct RunResult { float maxDiff = 0.0f; float lastBlockRatio = 0.0f; };

    // Runs numBlocks through the processor and compares output with input.
    RunResult run (DiceVoxProcessor& p)
    {
        p.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;
        RunResult r;

        for (int b = 0; b < numBlocks; ++b)
        {
            auto input  = makeSignal (blockSize, b * blockSize);
            auto buffer = input;
            p.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    r.maxDiff = std::max (r.maxDiff, std::abs (buffer.getSample (ch, i) - input.getSample (ch, i)));

            if (b == numBlocks - 1)
                r.lastBlockRatio = buffer.getRMSLevel (0, 0, blockSize) / input.getRMSLevel (0, 0, blockSize);
        }

        p.releaseResources();
        return r;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "DiceVox offline tests\n";

    {
        DiceVoxProcessor p;
        auto r = run (p);
        check (r.maxDiff < 1.0e-6f, "Default settings pass audio through unchanged",
               "max diff " + std::to_string (r.maxDiff));
    }
    {
        DiceVoxProcessor p;
        setParam (p, ParamIDs::mix, 0.0f);
        auto r = run (p);
        check (r.maxDiff < 1.0e-6f, "Mix at 0% is clean dry signal",
               "max diff " + std::to_string (r.maxDiff));
    }
    {
        DiceVoxProcessor p;
        setParam (p, ParamIDs::outputGain, -6.0f);
        auto r = run (p);
        const float expected = juce::Decibels::decibelsToGain (-6.0f);
        check (std::abs (r.lastBlockRatio - expected) < 0.01f, "Output -6 dB halves the level",
               "ratio " + std::to_string (r.lastBlockRatio) + ", expected " + std::to_string (expected));
    }
    {
        DiceVoxProcessor p;
        setParam (p, ParamIDs::inputGain, 6.0f);
        auto r = run (p);
        const float expected = juce::Decibels::decibelsToGain (6.0f);
        check (std::abs (r.lastBlockRatio - expected) < 0.02f, "Input +6 dB doubles the level",
               "ratio " + std::to_string (r.lastBlockRatio) + ", expected " + std::to_string (expected));
    }
    {
        DiceVoxProcessor p;
        setParam (p, ParamIDs::outputGain, 12.0f);
        setParam (p, ParamIDs::bypass, 1.0f);
        auto r = run (p);
        check (r.maxDiff < 1.0e-6f, "Bypass ignores all settings",
               "max diff " + std::to_string (r.maxDiff));
    }
    {
        juce::MemoryBlock saved;
        {
            DiceVoxProcessor p;
            setParam (p, ParamIDs::outputGain, -3.5f);
            setParam (p, ParamIDs::mix, 42.0f);
            p.getStateInformation (saved);
        }
        DiceVoxProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        const float out = restored.state.getRawParameterValue (ParamIDs::outputGain)->load();
        const float mix = restored.state.getRawParameterValue (ParamIDs::mix)->load();
        check (std::abs (out + 3.5f) < 0.05f && std::abs (mix - 42.0f) < 0.05f,
               "Settings survive save/reload (FL project recall)",
               "output " + std::to_string (out) + " dB, mix " + std::to_string (mix) + "%");
    }
    {
        DiceVoxProcessor p;
        juce::AudioProcessor::BusesLayout mono, stereo, mismatch;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        mismatch.inputBuses.add (juce::AudioChannelSet::mono());
        mismatch.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.isBusesLayoutSupported (mono) && p.isBusesLayoutSupported (stereo)
                   && ! p.isBusesLayoutSupported (mismatch),
               "Accepts mono and stereo tracks");
    }
    {
        // Hosts identify the plugin by these codes. Changing them makes saved
        // FL Studio projects lose DiceVox, so this test locks them in place.
        auto fourCC = [] (int code)
        {
            std::string s;
            for (int shift = 24; shift >= 0; shift -= 8)
                s += (char) ((code >> shift) & 0xff);
            return s;
        };
        const auto manufacturer = fourCC (JucePlugin_ManufacturerCode);
        const auto plugin       = fourCC (JucePlugin_PluginCode);
        check (manufacturer == "Ahen" && plugin == "Dvox" && std::string (JucePlugin_Manufacturer) == "AHE",
               "Plugin identity is AHE / Ahen / Dvox (never change after release)",
               std::string (JucePlugin_Manufacturer) + " / " + manufacturer + " / " + plugin);
    }
    {
        DiceVoxProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        check (editor != nullptr && editor->getWidth() > 0, "Editor window opens");
    }

    std::cout << (failures == 0 ? "All tests passed.\n" : std::to_string (failures) + " test(s) failed.\n");
    return failures == 0 ? 0 : 1;
}
