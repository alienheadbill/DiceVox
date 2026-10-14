// Offline tests: run the plugin's audio engine without a DAW.
// Build target "DiceVoxTests" and run it; exit code 0 = all passed.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"
#include "ahe/AHEBrandMarks.h"
#include "ui/RollChamber.h"

#include <cmath>
#include <cstdlib>
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
        if (! ok)
        {
            ++failures;
            // In GitHub Actions, surface the failure as an annotation (readable without the full log).
            if (std::getenv ("GITHUB_ACTIONS") != nullptr)
                std::cout << "::error title=" << name << "::" << detail << "\n";
        }
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

    // ---------------------------------------------------------------------
    // Compatibility guards: the UI work must not change what hosts/projects see
    // ---------------------------------------------------------------------
    {
        DiceVoxProcessor p;
        juce::StringArray ids;
        for (auto* param : p.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                ids.add (ranged->getParameterID());
        check (ids.joinIntoString (",") == "input_gain,output_gain,mix,bypass",
               "Parameter IDs and order unchanged", ids.joinIntoString (",").toStdString());
    }
    {
        // Exact default state XML captured from main (281c9f6) before the UI work.
        DiceVoxProcessor p;
        const auto xml = p.state.copyState().toXmlString (juce::XmlElement::TextFormat().singleLine());
        const juce::String expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?> <DiceVoxState><PARAM id=\"bypass\" value=\"0.0\"/>"
                                      "<PARAM id=\"input_gain\" value=\"0.0\"/><PARAM id=\"mix\" value=\"100.0\"/>"
                                      "<PARAM id=\"output_gain\" value=\"0.0\"/></DiceVoxState>";
        check (xml == expected, "Saved-state format unchanged (matches main)", xml.toStdString());
    }

    // ---------------------------------------------------------------------
    // Editor: every restyled control is still wired to its parameter
    // ---------------------------------------------------------------------
    {
        std::function<juce::Component* (juce::Component&, const juce::String&)> findById =
            [&] (juce::Component& root, const juce::String& id) -> juce::Component*
        {
            for (auto* child : root.getChildren())
            {
                if (child->getComponentID() == id) return child;
                if (auto* hit = findById (*child, id)) return hit;
            }
            return nullptr;
        };

        DiceVoxProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        auto value = [&] (const char* id) { return p.state.getRawParameterValue (id)->load(); };

        struct KnobCase { const char* id; double uiValue; const char* unit; };
        for (auto kc : { KnobCase { ParamIDs::inputGain, -7.5, " dB" },
                         KnobCase { ParamIDs::mix, 35.0, " %" },
                         KnobCase { ParamIDs::outputGain, 4.2, " dB" } })
        {
            auto* slider = dynamic_cast<juce::Slider*> (findById (*editor, kc.id));
            if (slider == nullptr)
            {
                check (false, std::string ("Knob for ") + kc.id + " exists");
                continue;
            }

            // UI -> parameter
            slider->setValue (kc.uiValue, juce::sendNotificationSync);
            const bool uiToParam = std::abs (value (kc.id) - (float) kc.uiValue) < 0.051f;

            // Host automation -> UI
            auto* param = p.state.getParameter (kc.id);
            param->setValueNotifyingHost (param->convertTo0to1 (param->convertFrom0to1 (param->getDefaultValue())));
            const bool paramToUi = std::abs (slider->getValue() - (double) param->convertFrom0to1 (param->getDefaultValue())) < 0.051;

            // Units shown, typed values with the unit are understood, double-click resets to default
            const bool unitShown = slider->getTextFromValue (slider->getValue()).endsWith (kc.unit);
            const bool typedOk   = std::abs (slider->getValueFromText ("3.0" + juce::String (kc.unit)) - 3.0) < 0.051;
            const bool resetOk   = slider->isDoubleClickReturnEnabled()
                                && std::abs (slider->getDoubleClickReturnValue() - (double) param->convertFrom0to1 (param->getDefaultValue())) < 1.0e-6;

            check (uiToParam && paramToUi && unitShown && typedOk && resetOk,
                   std::string ("Knob '") + kc.id + "' drives its parameter, follows automation, shows units, resets on double-click",
                   "ui->param " + std::to_string (uiToParam) + ", param->ui " + std::to_string (paramToUi)
                       + ", unit " + std::to_string (unitShown) + ", typed " + std::to_string (typedOk)
                       + ", reset " + std::to_string (resetOk));
        }

        auto* bypass = dynamic_cast<juce::Button*> (findById (*editor, ParamIDs::bypass));
        bool bypassOk = false;
        if (bypass != nullptr)
        {
            bypass->setToggleState (true, juce::sendNotificationSync);
            const bool on = value (ParamIDs::bypass) > 0.5f;
            p.state.getParameter (ParamIDs::bypass)->setValueNotifyingHost (0.0f);
            bypassOk = on && ! bypass->getToggleState();
        }
        check (bypassOk, "Bypass switch drives the bypass parameter and follows automation");
    }
    {
        // ROLL must not be clickable/enabled-looking in Phase 1; ADVANCED tab is disabled.
        DiceVoxProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());

        std::function<void (juce::Component&, std::function<void (juce::Component&)>)> visit =
            [&] (juce::Component& c, std::function<void (juce::Component&)> fn) { fn (c); for (auto* ch : c.getChildren()) visit (*ch, fn); };

        bool rollFound = false, rollHasButton = false, advancedDisabled = false;
        visit (*editor, [&] (juce::Component& c)
        {
            if (auto* roll = dynamic_cast<RollChamber*> (&c))
            {
                rollFound = true;
                visit (*roll, [&] (juce::Component& inner) { rollHasButton |= dynamic_cast<juce::Button*> (&inner) != nullptr; });
            }
            if (auto* b = dynamic_cast<juce::TextButton*> (&c))
                if (b->getButtonText() == "ADVANCED")
                    advancedDisabled = ! b->isEnabled();
        });
        check (rollFound && ! rollHasButton, "ROLL chamber is shown but not clickable (randomizer is Phase 3)");
        check (advancedDisabled, "ADVANCED tab is disabled (Phase 5)");

        check (editor->getWidth() == 520 && editor->getHeight() == 360, "Editor opens at compact 520x360",
               std::to_string (editor->getWidth()) + "x" + std::to_string (editor->getHeight()));
    }
    {
        // Brand marks load from embedded SVGs; the lockup's "AUDIO" text is visible (non-zero size).
        auto icon = ahe::brand::alienIcon();
        auto ufo  = ahe::brand::ufoIcon();
        auto lock = ahe::brand::lockup();

        float audioHeight = 0.0f;
        if (lock != nullptr)
        {
            std::function<void (juce::Component&)> find = [&] (juce::Component& c)
            {
                if (auto* t = dynamic_cast<juce::DrawableText*> (&c))
                    if (t->getText() == "AUDIO")
                        audioHeight = t->getBoundingBox().getBoundingBox().getHeight();
                for (auto* ch : c.getChildren()) find (*ch);
            };
            find (*lock);
        }
        check (icon != nullptr && ufo != nullptr && lock != nullptr, "Brand marks load from embedded SVGs");
        check (audioHeight > 1.0f, "Lockup 'AUDIO' text renders (SVG text attributes applied)",
               "height " + std::to_string (audioHeight));
    }

    std::cout << (failures == 0 ? "All tests passed.\n" : std::to_string (failures) + " test(s) failed.\n");
    return failures == 0 ? 0 : 1;
}
