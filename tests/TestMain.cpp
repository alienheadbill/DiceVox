// Offline tests: run the plugin's audio engine without a DAW.
// Build target "DiceVoxTests" and run it; exit code 0 = all passed.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"
#include "modules/FilterModule.h"
#include "ahe/AHEBrandMarks.h"
#include "ahe/AHEFonts.h"
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

    // -----------------------------------------------------------------
    // Phase 2 helpers: measure what the effects actually do to audio
    // -----------------------------------------------------------------
    juce::AudioBuffer<float> sine (int numChannels, int numSamples, double freq, double sr, float amp = 0.5f)
    {
        juce::AudioBuffer<float> buf (numChannels, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto v = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr);
            for (int ch = 0; ch < numChannels; ++ch)
                buf.setSample (ch, i, v);
        }
        return buf;
    }

    juce::AudioBuffer<float> noise (int numChannels, int numSamples, float amp, int seed)
    {
        juce::AudioBuffer<float> buf (numChannels, numSamples);
        juce::Random rng (seed);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buf.setSample (ch, i, amp * (rng.nextFloat() * 2.0f - 1.0f));
        return buf;
    }

    // Runs `input` through the processor like a host would. blockSizeFor(b) picks
    // each block's length (default: the prepared size); beforeBlock(b) can move
    // parameters between blocks, like host automation.
    juce::AudioBuffer<float> processAll (DiceVoxProcessor& p, const juce::AudioBuffer<float>& input, double sr,
                                         int preparedBlock = blockSize,
                                         std::function<int (int)> blockSizeFor = {},
                                         std::function<void (int)> beforeBlock = {})
    {
        p.prepareToPlay (sr, preparedBlock);
        juce::AudioBuffer<float> out (input);
        juce::MidiBuffer midi;

        for (int pos = 0, b = 0; pos < out.getNumSamples(); ++b)
        {
            const int n = juce::jmin (blockSizeFor ? blockSizeFor (b) : preparedBlock, out.getNumSamples() - pos);
            if (beforeBlock) beforeBlock (b);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
            p.processBlock (view, midi);
            pos += n;
        }

        p.releaseResources();
        return out;
    }

    // Steady-state gain (dB) of a sine at `freq`, measured over whole periods at the end.
    float gainDb (DiceVoxProcessor& p, double freq, double sr, int numChannels = 2)
    {
        const int total = (int) (0.6 * sr);
        auto in  = sine (numChannels, total, freq, sr);
        auto out = processAll (p, in, sr);
        const int periods = juce::jmax (2, (int) (0.1 * freq));
        const int len = juce::roundToInt (periods * sr / freq);
        return juce::Decibels::gainToDecibels (out.getRMSLevel (0, total - len, len) / in.getRMSLevel (0, total - len, len), -200.0f);
    }

    void setFilter (DiceVoxProcessor& p, float lowCut, float highCut, float resonance = 0.0f)
    {
        setParam (p, "filter_on", 1.0f);
        setParam (p, FilterModule::lowCutId, lowCut);
        setParam (p, FilterModule::highCutId, highCut);
        setParam (p, FilterModule::resonanceId, resonance);
    }

    // Largest sample-to-sample jump on channel 0: a click shows up as a jump far
    // bigger than the clean signal ever makes.
    float maxStep (const juce::AudioBuffer<float>& b)
    {
        float m = 0.0f;
        for (int i = 1; i < b.getNumSamples(); ++i)
            m = std::max (m, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
        return m;
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    std::string db (float v) { return juce::String (v, 2).toStdString() + " dB"; }
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
        // Phase 1 parameters stay first, in their original order. Module
        // parameters are appended after them (Phase 2A: filter).
        check (ids.joinIntoString (",") == "input_gain,output_gain,mix,bypass,"
                                           "filter_on,filter_lowcut,filter_highcut,filter_resonance",
               "Parameter IDs and order: Phase 1 unchanged, filter appended", ids.joinIntoString (",").toStdString());
    }
    {
        // Default state captured from main (281c9f6) before the UI work. The four
        // Phase 1 entries must keep their tag, IDs, attributes and values; module
        // parameters are new PARAM entries beside them (JUCE stores them sorted by
        // ID) and every module starts off. Values are compared numerically because
        // float formatting differs by platform (Apple Silicon stores the 0 dB
        // default as ~3.6e-7, an inaudible rounding difference).
        DiceVoxProcessor p;
        auto xml = p.state.copyState().createXml();

        struct Expected { const char* id; double value; };
        const Expected expected[] = { { "bypass", 0.0 }, { "input_gain", 0.0 }, { "mix", 100.0 }, { "output_gain", 0.0 },
                                      { "filter_on", 0.0 }, { "filter_lowcut", 80.0 }, { "filter_highcut", 20000.0 },
                                      { "filter_resonance", 0.0 } };

        bool ok = xml != nullptr && xml->getTagName() == "DiceVoxState"
               && xml->getNumChildElements() == (int) std::size (expected)
               && xml->getNumChildElements() == p.getParameters().size();
        juce::String detail = xml != nullptr ? xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader()) : "no xml";

        for (auto& e : expected)
        {
            auto* el = ok ? xml->getChildByAttribute ("id", e.id) : nullptr;
            ok = el != nullptr
              && el->getTagName() == "PARAM"
              && el->getNumAttributes() == 2
              && std::abs (el->getDoubleAttribute ("value") - e.value) < 1.0e-4;
        }
        check (ok, "Saved-state format: Phase 1 entries unchanged, modules default off", detail.toStdString());
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

        // At the minimum (520x360) size the tabs must keep their full width and
        // stay clear of the product name and maker mark.
        juce::Rectangle<int> quickR, advancedR;
        visit (*editor, [&] (juce::Component& c)
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (&c))
            {
                if (b->getButtonText() == "QUICK")    quickR    = b->getBoundsInParent();
                if (b->getButtonText() == "ADVANCED") advancedR = b->getBoundsInParent();
            }
        });
        check (quickR.getWidth() >= 64 && advancedR.getWidth() >= 140 && ! quickR.intersects (advancedR)
                   && advancedR.getRight() <= 520 - 16 - 90,
               "QUICK / ADVANCED tabs are not crowded at minimum size",
               "QUICK " + quickR.toString().toStdString() + ", ADVANCED " + advancedR.toString().toStdString());

        check (editor->getWidth() == 520 && editor->getHeight() == 360, "Editor opens at compact 520x360",
               std::to_string (editor->getWidth()) + "x" + std::to_string (editor->getHeight()));
    }
    {
        // Brand marks load from embedded SVGs, including the compact AHE wordmark
        // (vector shapes from the lockup, wider than tall).
        auto icon = ahe::brand::alienIcon();
        auto ufo  = ahe::brand::ufoIcon();
        auto word = ahe::brand::wordmark();
        const bool wordOk = word != nullptr && word->getDrawableBounds().getWidth() > word->getDrawableBounds().getHeight() * 2.0f;
        check (icon != nullptr && ufo != nullptr && wordOk,
               "Brand marks load from embedded SVGs (incl. compact AHE wordmark)");
    }

    {
        // Embedded fonts load, so the UI looks identical on every computer.
        // Platforms split "family" and "style" differently (Windows reports
        // family "Barlow", style "Condensed ExtraBold Italic"), so check the
        // combined family + style text.
        auto describe = [] (const juce::Typeface::Ptr& t)
        {
            return t == nullptr ? juce::String ("null") : "[" + t->getName() + " | " + t->getStyle() + "]";
        };
        auto regular = ahe::fonts::uiRegular(), semi = ahe::fonts::uiSemiBold(), disp = ahe::fonts::display();
        auto fullName = [] (const juce::Typeface::Ptr& t) { return t->getName() + " " + t->getStyle(); };
        const bool ok = regular != nullptr && semi != nullptr && disp != nullptr
                     && fullName (regular).contains ("Inter") && fullName (regular).contains ("Regular")
                     && fullName (semi).contains ("Inter")    && fullName (semi).contains ("SemiBold")
                     && fullName (disp).contains ("Barlow")   && fullName (disp).contains ("Condensed")
                     && fullName (disp).contains ("ExtraBold") && fullName (disp).contains ("Italic");
        check (ok, "Embedded fonts load: Inter Regular, Inter SemiBold, Barlow Condensed",
               (describe (regular) + " " + describe (semi) + " " + describe (disp)).toStdString());
    }

    // ---------------------------------------------------------------------
    // Phase 2A: filter (low cut + high cut, 12 dB/oct, shared resonance)
    // Measured on real test signals, not just parameter wiring.
    // ---------------------------------------------------------------------
    {
        DiceVoxProcessor p;
        setParam (p, "filter_on", 1.0f); // default settings: 80 Hz low cut, high cut open
        const float at1k = gainDb (p, 1000.0, sampleRate);
        const float at30 = gainDb (p, 30.0, sampleRate);
        check (std::abs (at1k) < 0.3f && at30 < -12.0f, "Filter defaults: 80 Hz rumble cut, vocal range untouched",
               "1 kHz " + db (at1k) + ", 30 Hz " + db (at30));
    }
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const auto rate = std::to_string ((int) sr);
        {
            DiceVoxProcessor p;
            setFilter (p, 500.0f, 20000.0f);
            const float atCut = gainDb (p, 500.0, sr), below = gainDb (p, 100.0, sr), pass = gainDb (p, 4000.0, sr);
            check (std::abs (atCut + 3.01f) < 0.5f && below < -20.0f && std::abs (pass) < 0.5f,
                   "Low cut 500 Hz @ " + rate + ": -3 dB at cutoff, 100 Hz cut, 4 kHz passes",
                   "500 Hz " + db (atCut) + ", 100 Hz " + db (below) + ", 4 kHz " + db (pass));
        }
        {
            DiceVoxProcessor p;
            setFilter (p, 20.0f, 2000.0f);
            const float atCut = gainDb (p, 2000.0, sr), above = gainDb (p, 10000.0, sr), pass = gainDb (p, 200.0, sr);
            check (std::abs (atCut + 3.01f) < 0.5f && above < -20.0f && std::abs (pass) < 0.5f,
                   "High cut 2 kHz @ " + rate + ": -3 dB at cutoff, 10 kHz cut, 200 Hz passes",
                   "2 kHz " + db (atCut) + ", 10 kHz " + db (above) + ", 200 Hz " + db (pass));
        }
    }
    {
        // "Telephone" band: the kind of vocal transformation the randomizer will use.
        DiceVoxProcessor p;
        setFilter (p, 300.0f, 3400.0f);
        const float mid = gainDb (p, 1000.0, sampleRate), lo = gainDb (p, 100.0, sampleRate), hi = gainDb (p, 10000.0, sampleRate);
        check (std::abs (mid) < 1.0f && lo < -15.0f && hi < -15.0f, "Telephone band (300 Hz - 3.4 kHz) keeps the voice, cuts lows and highs",
               "1 kHz " + db (mid) + ", 100 Hz " + db (lo) + ", 10 kHz " + db (hi));
    }
    {
        // Full resonance: a peak of 20*log10(maxQ) (about +8 dB) at the cutoff, never more.
        DiceVoxProcessor p;
        setFilter (p, 20.0f, 2000.0f, 100.0f);
        const float expected = juce::Decibels::gainToDecibels (FilterModule::maxQ);
        float peak = -100.0f;
        for (double f : { 1500.0, 1800.0, 2000.0, 2200.0 })
            peak = std::max (peak, gainDb (p, f, sampleRate));
        const float atCut = gainDb (p, 2000.0, sampleRate);
        check (std::abs (atCut - expected) < 0.5f && peak < expected + 1.0f, "Full resonance peaks at the cutoff, capped near +8 dB",
               "at cutoff " + db (atCut) + ", expected " + db (expected) + ", highest " + db (peak));
    }
    {
        // Extreme automation: every block jumps all filter settings, toggles the
        // filter, and changes block size (1..1024, prepared for 512).
        DiceVoxProcessor p;
        juce::Random rng (99);
        auto in  = noise (2, (int) (4.0 * sampleRate), 0.3f, 7);
        auto out = processAll (p, in, sampleRate, blockSize,
                               [&] (int) { return 1 + rng.nextInt (1024); },
                               [&] (int b)
                               {
                                   setParam (p, "filter_on", (b / 7) % 2 == 0 ? 1.0f : 0.0f);
                                   setParam (p, FilterModule::lowCutId,    20.0f * std::pow (100.0f, rng.nextFloat()));
                                   setParam (p, FilterModule::highCutId,  200.0f * std::pow (100.0f, rng.nextFloat()));
                                   setParam (p, FilterModule::resonanceId, 100.0f * rng.nextFloat());
                               });
        const float peak = out.getMagnitude (0, out.getNumSamples());
        check (allFinite (out) && peak < 1.5f, "Filter stays stable under extreme automation (no NaN/Inf, no blow-up)",
               "peak " + std::to_string (peak) + " for input peak 0.3");
    }
    {
        // Switching the filter on and off mid-signal must not click.
        DiceVoxProcessor p;
        setParam (p, FilterModule::lowCutId, 2000.0f); // filtered sound is very different from dry
        auto in  = sine (2, 60 * blockSize, 1000.0, sampleRate);
        auto out = processAll (p, in, sampleRate, blockSize, {},
                               [&] (int b) { if (b == 20) setParam (p, "filter_on", 1.0f);
                                             if (b == 40) setParam (p, "filter_on", 0.0f); });
        const float dryStep = maxStep (in), step = maxStep (out);
        check (step < 1.25f * dryStep, "Switching the filter on/off is click-free (crossfade)",
               "max step " + std::to_string (step) + ", clean sine " + std::to_string (dryStep));
    }
    {
        // Big cutoff jumps (automation, future dice rolls) glide over ~50 ms instead
        // of snapping, and stay click-free.
        DiceVoxProcessor p;
        setFilter (p, 20.0f, 20000.0f);
        auto in  = sine (2, 60 * blockSize, 1000.0, sampleRate);
        auto out = processAll (p, in, sampleRate, blockSize, {},
                               [&] (int b) { if (b == 20) setParam (p, FilterModule::highCutId, 200.0f);
                                             if (b == 40) { setParam (p, FilterModule::lowCutId, 2000.0f); setParam (p, FilterModule::highCutId, 20000.0f); } });
        auto levelDb = [&] (int start, int len) { return juce::Decibels::gainToDecibels (out.getRMSLevel (0, start, len) / in.getRMSLevel (0, start, len)); };
        const int jump = 20 * blockSize;
        const float justAfter = levelDb (jump, 240), settled = levelDb (jump + 7200, 480);
        const float dryStep = maxStep (in), step = maxStep (out);
        check (allFinite (out) && step < 1.25f * dryStep && justAfter > -3.0f && settled < -20.0f,
               "Cutoff jumps glide (smoothed) and stay click-free",
               "first 5 ms " + db (justAfter) + ", after 150 ms " + db (settled)
                   + ", max step " + std::to_string (step) + ", clean sine " + std::to_string (dryStep));
    }
    {
        // Same audio whatever the host's block sizes, including a first block bigger
        // than prepared while the filter fades in and its cutoff is gliding.
        auto in = noise (2, (int) sampleRate, 0.3f, 11);
        auto render = [&] (std::function<int (int)> sizes)
        {
            DiceVoxProcessor p;
            return processAll (p, in, sampleRate, blockSize, sizes,
                               [&] (int b) { if (b == 0) setFilter (p, 1200.0f, 3000.0f, 60.0f); });
        };
        juce::Random rng (5);
        auto fixedBlocks  = render ({});
        auto randomBlocks = render ([&] (int b) { return b == 0 ? 1500 : 1 + rng.nextInt (700); });
        float diff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < in.getNumSamples(); ++i)
                diff = std::max (diff, std::abs (fixedBlocks.getSample (ch, i) - randomBlocks.getSample (ch, i)));
        check (diff < 1.0e-5f, "Filter output does not depend on host block size", "max diff " + std::to_string (diff));
    }
    {
        // Silence stays silence (no noise, no denormal build-up), even after a burst.
        DiceVoxProcessor p;
        setFilter (p, 500.0f, 3000.0f, 100.0f);
        juce::AudioBuffer<float> in (2, (int) (2.0 * sampleRate));
        in.clear();
        in.setSample (0, 100, 1.0f);
        in.setSample (1, 100, 1.0f);
        auto out = processAll (p, in, sampleRate);
        const float tail = out.getMagnitude ((int) (1.5 * sampleRate), (int) (0.5 * sampleRate));
        check (allFinite (out) && tail < 1.0e-15f, "Filter decays to true silence after input stops",
               "tail magnitude " + std::to_string (tail));
    }
    {
        // Mono tracks.
        DiceVoxProcessor p;
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        const bool layoutOk = p.setBusesLayout (mono);
        setFilter (p, 20.0f, 2000.0f);
        const float pass = gainDb (p, 200.0, sampleRate, 1), cut = gainDb (p, 10000.0, sampleRate, 1);
        check (layoutOk && std::abs (pass) < 0.5f && cut < -20.0f, "Filter works on mono tracks",
               "200 Hz " + db (pass) + ", 10 kHz " + db (cut));
    }
    {
        DiceVoxProcessor off, on;
        setFilter (on, 500.0f, 3000.0f, 50.0f);
        off.prepareToPlay (sampleRate, blockSize);
        on.prepareToPlay (sampleRate, blockSize);
        check (off.getLatencySamples() == 0 && on.getLatencySamples() == 0, "Filter adds no latency (host delay compensation unchanged)",
               std::to_string (off.getLatencySamples()) + " / " + std::to_string (on.getLatencySamples()) + " samples");
    }
    {
        DiceVoxProcessor p;
        setFilter (p, 2000.0f, 3000.0f, 100.0f);
        setParam (p, ParamIDs::bypass, 1.0f);
        const auto bypassDiff = run (p).maxDiff;

        DiceVoxProcessor q;
        setFilter (q, 2000.0f, 3000.0f, 100.0f);
        setParam (q, ParamIDs::mix, 0.0f);
        const auto dryDiff = run (q).maxDiff;
        check (bypassDiff < 1.0e-6f && dryDiff < 1.0e-6f, "Bypass and Mix 0% stay clean with the filter on",
               "bypass diff " + std::to_string (bypassDiff) + ", mix 0 diff " + std::to_string (dryDiff));
    }
    {
        // A project saved with the Phase 1 build (only the four original
        // parameters) must open and sound exactly as before: filter off, at
        // defaults, even if this instance had it on before loading.
        const char* phase1Xml = R"(<?xml version="1.0" encoding="UTF-8"?>)"
                                R"(<DiceVoxState><PARAM id="bypass" value="0.0"/><PARAM id="input_gain" value="0.0"/>)"
                                R"(<PARAM id="mix" value="42.0"/><PARAM id="output_gain" value="-3.5"/></DiceVoxState>)";
        juce::MemoryBlock saved;
        juce::AudioProcessor::copyXmlToBinary (*juce::parseXML (juce::String (phase1Xml)), saved);

        DiceVoxProcessor p;
        setFilter (p, 2000.0f, 3000.0f, 100.0f);
        p.setStateInformation (saved.getData(), (int) saved.getSize());
        auto v = [&] (const char* id) { return p.state.getRawParameterValue (id)->load(); };

        const bool valuesOk = std::abs (v (ParamIDs::mix) - 42.0f) < 0.05f && std::abs (v (ParamIDs::outputGain) + 3.5f) < 0.05f
                           && v ("filter_on") < 0.5f && std::abs (v (FilterModule::lowCutId) - 80.0f) < 0.01f
                           && std::abs (v (FilterModule::highCutId) - 20000.0f) < 0.5f && v (FilterModule::resonanceId) < 0.01f;
        auto r = run (p);
        const float expected = juce::Decibels::decibelsToGain (-3.5f);
        check (valuesOk && std::abs (r.lastBlockRatio - expected) < 0.01f,
               "Phase 1 project loads unchanged: same settings, filter off, same sound",
               "level ratio " + std::to_string (r.lastBlockRatio) + ", expected " + std::to_string (expected));
    }
    {
        juce::MemoryBlock saved;
        {
            DiceVoxProcessor p;
            setFilter (p, 333.0f, 4444.0f, 55.0f);
            p.getStateInformation (saved);
        }
        DiceVoxProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        auto v = [&] (const char* id) { return restored.state.getRawParameterValue (id)->load(); };
        check (v ("filter_on") > 0.5f && std::abs (v (FilterModule::lowCutId) - 333.0f) < 0.5f
                   && std::abs (v (FilterModule::highCutId) - 4444.0f) < 1.0f && std::abs (v (FilterModule::resonanceId) - 55.0f) < 0.06f,
               "Filter settings survive save/reload");
    }

    {
        // CPU cost (informational, not a pass/fail): 10 s of stereo audio at 48 kHz
        // with the filter on, settled and while every cutoff is gliding.
        auto in = noise (2, (int) (10.0 * sampleRate), 0.3f, 3);
        auto timeIt = [&] (bool gliding)
        {
            DiceVoxProcessor p;
            setFilter (p, 300.0f, 3400.0f, 40.0f);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            processAll (p, in, sampleRate, blockSize, {},
                        [&] (int b) { if (gliding) setParam (p, FilterModule::lowCutId, b % 2 == 0 ? 100.0f : 1000.0f); });
            return (juce::Time::getMillisecondCounterHiRes() - t0) / 10000.0 * 100.0;
        };
        const double settled = timeIt (false), gliding = timeIt (true);
        std::cout << "  INFO  Filter CPU, one core: " << juce::String (settled, 3) << " % of real time settled, "
                  << juce::String (gliding, 3) << " % while cutoffs glide\n";
    }
    std::cout << (failures == 0 ? "All tests passed.\n" : std::to_string (failures) + " test(s) failed.\n");
    return failures == 0 ? 0 : 1;
}
