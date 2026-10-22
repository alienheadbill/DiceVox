// Offline tests: run the plugin's audio engine without a DAW.
// Build target "DiceVoxTests" and run it; exit code 0 = all passed.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"
#include "modules/FilterModule.h"
#include "modules/DriveModule.h"
#include "modules/ChorusModule.h"
#include "modules/DelayModule.h"
#include "modules/ReverbModule.h"
#include "modules/PitchModule.h"
#include "modules/LoFiModule.h"
#include "modules/ChopModule.h"
#include "modules/ReverseModule.h"
#include "dice/DiceRoller.h"
#include "ahe/AHEBrandMarks.h"
#include "ahe/AHEFonts.h"
#include "ui/RollChamber.h"
#include "ui/PresetBar.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <tuple>

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

    // Full frequency response (dB per FFT bin) of the plugin at its current
    // settings: the FFT of its impulse response (settings are applied before
    // prepare, so nothing is gliding).
    struct Response { std::vector<float> db; double binHz = 0.0; };
    Response impulseResponse (DiceVoxProcessor& p, double sr, int numChannels = 2)
    {
        constexpr int order = 15, size = 1 << order;
        juce::AudioBuffer<float> in (numChannels, size);
        in.clear();
        for (int ch = 0; ch < numChannels; ++ch)
            in.setSample (ch, 0, 1.0f);
        auto out = processAll (p, in, sr);

        std::vector<float> data (2 * size, 0.0f);
        std::copy (out.getReadPointer (0), out.getReadPointer (0) + size, data.begin());
        juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (data.data());

        Response r;
        r.binHz = sr / size;
        for (int k = 0; k <= size / 2; ++k)
            r.db.push_back (juce::Decibels::gainToDecibels (data[(size_t) k], -200.0f));
        return r;
    }

    // Highest point of one resonant 12 dB/oct stage at this Q (Q / sqrt(1 - 1/(4Q^2))).
    float singleStagePeakDb (float q)
    {
        return q <= FilterModule::minQ + 1.0e-4f ? 0.0f
                                                 : juce::Decibels::gainToDecibels (q / std::sqrt (1.0f - 1.0f / (4.0f * q * q)));
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
        // Phase 1 parameters stay first, in their original order. Module
        // parameters are appended after them, in chain order.
        check (ids.joinIntoString (",") == "input_gain,output_gain,mix,bypass,"
                                           "filter_on,filter_lowcut,filter_highcut,filter_resonance,"
                                           "drive_on,drive_amount,drive_tone,"
                                           "chorus_on,chorus_rate,chorus_depth,"
                                           "delay_on,delay_time,delay_feedback,delay_mix,"
                                           "reverb_on,reverb_size,reverb_mix,"
                                           "pitch_on,pitch_shift,pitch_mix,"
                                           "lofi_on,lofi_crush,lofi_wobble,lofi_mix,"
                                           "chop_on,chop_rate,chop_depth,"
                                           "reverse_on,reverse_length,reverse_mix,"
                                           "dice_intensity,dice_style,"
                                           "dice_lock_filter,dice_lock_drive,dice_lock_chorus,dice_lock_delay,dice_lock_reverb,"
                                           "dice_lock_pitch,dice_lock_lofi,dice_lock_chop,dice_lock_reverse",
               "Parameter IDs and order: Phase 1 unchanged, effects and dice controls appended", ids.joinIntoString (",").toStdString());
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
                                      { "filter_resonance", 0.0 },
                                      { "drive_on", 0.0 }, { "drive_amount", 35.0 }, { "drive_tone", 60.0 },
                                      { "chorus_on", 0.0 }, { "chorus_rate", 0.8 }, { "chorus_depth", 40.0 },
                                      { "delay_on", 0.0 }, { "delay_time", 5.0 }, { "delay_feedback", 35.0 }, { "delay_mix", 30.0 },
                                      { "reverb_on", 0.0 }, { "reverb_size", 50.0 }, { "reverb_mix", 25.0 },
                                      { "pitch_on", 0.0 }, { "pitch_shift", -12.0 }, { "pitch_mix", 40.0 },
                                      { "lofi_on", 0.0 }, { "lofi_crush", 50.0 }, { "lofi_wobble", 30.0 }, { "lofi_mix", 100.0 },
                                      { "chop_on", 0.0 }, { "chop_rate", 1.0 }, { "chop_depth", 80.0 },
                                      { "reverse_on", 0.0 }, { "reverse_length", 2.0 }, { "reverse_mix", 40.0 },
                                      { "dice_intensity", 50.0 }, { "dice_style", 0.0 },
                                      { "dice_lock_filter", 0.0 }, { "dice_lock_drive", 0.0 }, { "dice_lock_chorus", 0.0 },
                                      { "dice_lock_delay", 0.0 }, { "dice_lock_reverb", 0.0 }, { "dice_lock_pitch", 0.0 },
                                      { "dice_lock_lofi", 0.0 }, { "dice_lock_chop", 0.0 }, { "dice_lock_reverse", 0.0 } };

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
        // Every effect parameter has a control on screen, wired both ways.
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

        juce::StringArray missing, unwired;
        for (auto* param : p.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            const auto id = ranged->getParameterID();
            auto* control = findById (*editor, id);
            if (control == nullptr) { missing.add (id); continue; }

            // Host -> UI: move the parameter to the far end of its range and see the control follow.
            ranged->setValueNotifyingHost (1.0f);
            bool follows = false;
            if (auto* s = dynamic_cast<juce::Slider*> (control))
                follows = std::abs (s->getValue() - (double) ranged->convertFrom0to1 (1.0f)) < 0.051;
            else if (auto* b = dynamic_cast<juce::Button*> (control))
                follows = b->getToggleState();
            else if (auto* combo = dynamic_cast<juce::ComboBox*> (control))
                follows = combo->getSelectedItemIndex() == juce::roundToInt (ranged->convertFrom0to1 (1.0f));
            if (! follows) unwired.add (id);
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
        }
        check (missing.isEmpty() && unwired.isEmpty(), "Every parameter has an on-screen control that follows automation",
               "missing: " + missing.joinIntoString (",").toStdString() + "; not following: " + unwired.joinIntoString (",").toStdString());

        check (editor->getWidth() == 860 && editor->getHeight() == 690, "Editor opens at 860x690",
               std::to_string (editor->getWidth()) + "x" + std::to_string (editor->getHeight()));
    }
    {
        // ROLL and BACK work from the screen.
        DiceVoxProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());

        std::function<void (juce::Component&, std::function<void (juce::Component&)>)> visit =
            [&] (juce::Component& c, std::function<void (juce::Component&)> fn) { fn (c); for (auto* ch : c.getChildren()) visit (*ch, fn); };

        RollChamber* roll = nullptr;
        visit (*editor, [&] (juce::Component& c) { if (auto* r = dynamic_cast<RollChamber*> (&c)) roll = r; });

        bool ok = false;
        std::string detail = "no ROLL chamber";
        if (roll != nullptr)
        {
            auto countOn = [&]
            {
                int n = 0;
                for (auto* id : { "filter_on", "pitch_on", "drive_on", "lofi_on", "chorus_on", "chop_on", "reverse_on", "delay_on", "reverb_on" })
                    n += p.state.getRawParameterValue (id)->load() > 0.5f ? 1 : 0;
                return n;
            };
            const bool backDisabledFirst = ! roll->getBackButton().isEnabled();
            // Run the buttons' click handlers directly (a real click is delivered
            // asynchronously, and this test has no running message loop).
            roll->getRollButton().onClick();
            const int onAfterRoll = countOn();
            const bool backEnabled = roll->getBackButton().isEnabled();
            roll->getBackButton().onClick();
            const int onAfterBack = countOn();

            ok = backDisabledFirst && onAfterRoll >= 2 && onAfterRoll <= 4 && backEnabled && onAfterBack == 0
              && p.getRollName().isEmpty();
            detail = "effects on after ROLL " + std::to_string (onAfterRoll) + ", after BACK " + std::to_string (onAfterBack)
                   + ", BACK disabled at start " + std::to_string (backDisabledFirst) + ", enabled after roll " + std::to_string (backEnabled);
        }
        check (ok, "ROLL button builds a 2-4 effect chain, BACK restores the previous one", detail);
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
        // Resonance safety (review on PR #7): with the cutoffs close together both
        // resonant peaks land on the same frequencies and would multiply (Q x Q,
        // +15.9 dB). Measure the whole response (impulse + FFT) for equal, near,
        // crossed and wide cutoffs: the combined peak must stay within one stage's
        // peak at the dialled resonance (+8.14 dB at 100 %).
        struct Pair { float lowCut, highCut; };
        const Pair pairs[] = { { 800, 800 }, { 1000, 1000 }, { 1000, 1100 }, { 1000, 1200 }, { 1200, 1000 }, { 1000, 1400 },
                               { 1000, 2000 }, { 1000, 4000 }, { 300, 3400 }, { 200, 200 }, { 2000, 2000 }, { 2000, 200 },
                               { 20, 2000 }, { 80, 20000 } };
        // Optional: DICEVOX_RESPONSE_CSV=<file> writes these measured responses
        // (log-spaced points) for review charts.
        auto csvPath = juce::SystemStats::getEnvironmentVariable ("DICEVOX_RESPONSE_CSV", {});
        std::unique_ptr<juce::FileOutputStream> csv;
        if (csvPath.isNotEmpty())
        {
            juce::File (csvPath).deleteFile();
            csv = std::make_unique<juce::FileOutputStream> (juce::File (csvPath));
            *csv << "sample_rate,resonance,low_cut,high_cut,freq_hz,gain_db\n";
        }

        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            for (float res : { 100.0f, 50.0f })
            {
                const float limit = singleStagePeakDb (FilterModule::resonanceToQ (res));
                float worst = -100.0f;
                std::string worstAt;
                for (auto pr : pairs)
                {
                    DiceVoxProcessor p;
                    setFilter (p, pr.lowCut, pr.highCut, res);
                    auto r = impulseResponse (p, sr);
                    const auto k = (size_t) std::distance (r.db.begin(), std::max_element (r.db.begin(), r.db.end()));
                    const auto pairName = std::to_string ((int) pr.lowCut) + "/" + std::to_string ((int) pr.highCut);
                    if (r.db[k] > worst) { worst = r.db[k]; worstAt = pairName + " Hz, peak at " + std::to_string ((int) (k * r.binHz)) + " Hz"; }

                    if (csv != nullptr)
                        for (double f = 20.0; f < sr / 2; f *= 1.01)
                            *csv << juce::String (sr) << "," << juce::String (res) << "," << juce::String (pr.lowCut) << ","
                                 << juce::String (pr.highCut) << "," << juce::String (f, 1) << ","
                                 << juce::String (r.db[(size_t) juce::roundToInt (f / r.binHz)], 3) << "\n";
                }
                check (worst <= limit + 0.5f,
                       "Combined resonance peak capped (" + std::to_string ((int) res) + " %, " + std::to_string ((int) sr) + " Hz, equal/near/crossed cutoffs)",
                       "highest " + db (worst) + " at " + worstAt + ", limit " + db (limit));
            }
        }
    }
    for (int channels : { 2, 1 })
    {
        // Cutoffs gliding through each other at full resonance (automation or
        // dice): a sine sitting where they cross must never jump by more than
        // one stage's peak, and nothing may blow up. Stereo and mono.
        const auto layoutName = std::string (channels == 2 ? "stereo" : "mono");
        const float limit = singleStagePeakDb (FilterModule::maxQ);
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (channels == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono());
        layout.outputBuses.add (channels == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono());

        struct Sweep { const char* name; const char* fixedId; const char* movingId; };
        for (auto sw : { Sweep { "low cut through high cut", FilterModule::highCutId, FilterModule::lowCutId },
                         Sweep { "high cut through low cut", FilterModule::lowCutId, FilterModule::highCutId } })
        {
            DiceVoxProcessor p;
            p.setBusesLayout (layout);
            setFilter (p, 1000.0f, 1000.0f, 100.0f);
            setParam (p, sw.fixedId, 1000.0f);
            setParam (p, sw.movingId, 200.0f);
            const float amp = 0.25f;
            auto in  = sine (channels, (int) (4.0 * sampleRate), 1000.0, sampleRate, amp);
            // Moving cutoff: 200 Hz -> 5 kHz -> 200 Hz over 2 s (log), twice.
            auto out = processAll (p, in, sampleRate, blockSize, {},
                                   [&] (int b)
                                   {
                                       const double t = std::fmod (b * blockSize / sampleRate, 2.0);
                                       const double pos = t < 1.0 ? t : 2.0 - t;
                                       setParam (p, sw.movingId, (float) (200.0 * std::pow (25.0, pos)));
                                   });
            const float peakDb = juce::Decibels::gainToDecibels (out.getMagnitude (0, out.getNumSamples()) / amp);
            check (allFinite (out) && peakDb <= limit + 1.0f,
                   "Sweeping " + std::string (sw.name) + " at full resonance stays within one stage's peak (" + layoutName + ")",
                   "highest " + db (peakDb) + ", limit " + db (limit));
        }
        {
            // Rapid crossing: the two cutoffs swap places every 40 ms (300 Hz <-> 3 kHz),
            // noise in, random block sizes. Finite, bounded and click-free.
            DiceVoxProcessor p;
            p.setBusesLayout (layout);
            juce::Random rng (21);
            auto in  = noise (channels, (int) (3.0 * sampleRate), 0.25f, 31);
            int sinceSwap = 0, swaps = 0;
            auto out = processAll (p, in, sampleRate, blockSize,
                                   [&] (int) { return 64 + rng.nextInt (448); },
                                   [&] (int b)
                                   {
                                       if (b == 0) setFilter (p, 300.0f, 3000.0f, 100.0f);
                                       if (++sinceSwap * 256 > (int) (0.04 * sampleRate))
                                       {
                                           sinceSwap = 0;
                                           const bool crossed = (++swaps % 2) == 1;
                                           setParam (p, FilterModule::lowCutId,  crossed ? 3000.0f : 300.0f);
                                           setParam (p, FilterModule::highCutId, crossed ? 300.0f : 3000.0f);
                                       }
                                   });
            const float peak = out.getMagnitude (0, out.getNumSamples());
            check (allFinite (out) && peak < 1.0f,
                   "Cutoffs repeatedly crossing each other stay stable and bounded (" + layoutName + ")",
                   "peak " + std::to_string (peak) + " for input peak 0.25, " + std::to_string (swaps) + " swaps");
        }
    }
    {
        // The safety limit moves with the cutoffs, so it glides too: as the low
        // cut slides up to meet the high cut, the per-stage Q falls smoothly (no
        // jumps between neighbouring cutoff settings).
        float largestJump = 0.0f, qFar = 0.0f, qMet = 0.0f;
        float previous = FilterModule::stageQ (100.0f, 100.0f, 1000.0f, sampleRate);
        for (float lc = 100.0f; lc <= 1000.0f; lc *= 1.001f)
        {
            const float q = FilterModule::stageQ (100.0f, lc, 1000.0f, sampleRate);
            largestJump = std::max (largestJump, std::abs (q - previous));
            previous = q;
        }
        qFar = FilterModule::stageQ (100.0f, 20.0f, 2000.0f, sampleRate);
        qMet = FilterModule::stageQ (100.0f, 1000.0f, 1000.0f, sampleRate);
        check (largestJump < 0.01f && std::abs (qFar - FilterModule::maxQ) < 0.01f && qMet < 0.65f * FilterModule::maxQ,
               "Per-stage Q is full when cutoffs are far apart and glides down as they meet",
               "far " + std::to_string (qFar) + ", met " + std::to_string (qMet) + ", largest step " + std::to_string (largestJump));
    }
    // ---------------------------------------------------------------------
    // Usable v1: drive, chorus, delay, reverb, and the dice
    // ---------------------------------------------------------------------
    const char* const allEffects[] = { "filter_on", "pitch_on", "drive_on", "lofi_on", "chorus_on",
                                       "chop_on", "reverse_on", "delay_on", "reverb_on" };
    {
        // Drive: rounds off peaks and adds harmonics, without getting much louder.
        DiceVoxProcessor p;
        setParam (p, "drive_on", 1.0f);
        setParam (p, DriveModule::amountId, 80.0f);
        setParam (p, DriveModule::toneId, 100.0f);
        auto in  = sine (2, (int) sampleRate, 220.0, sampleRate, 0.5f);
        auto out = processAll (p, in, sampleRate);
        const int tail = (int) (0.5 * sampleRate);
        const float levelDb = juce::Decibels::gainToDecibels (out.getRMSLevel (0, tail, tail) / in.getRMSLevel (0, tail, tail));
        const float crest = out.getMagnitude (0, tail, tail) / out.getRMSLevel (0, tail, tail); // sine = 1.41, squarer = lower
        check (allFinite (out) && out.getMagnitude (0, out.getNumSamples()) < 1.0f && std::abs (levelDb) < 6.0f && crest < 1.36f,
               "Drive saturates (flatter peaks) without a big level jump",
               "level change " + db (levelDb) + ", crest factor " + std::to_string (crest));
    }
    {
        // Drive at 0 % with the tone open is close to transparent for quiet signals.
        DiceVoxProcessor p;
        setParam (p, "drive_on", 1.0f);
        setParam (p, DriveModule::amountId, 0.0f);
        setParam (p, DriveModule::toneId, 100.0f);
        auto in  = sine (2, (int) sampleRate, 1000.0, sampleRate, 0.05f);
        auto out = processAll (p, in, sampleRate);
        const int tail = (int) (0.5 * sampleRate);
        const float levelDb = juce::Decibels::gainToDecibels (out.getRMSLevel (0, tail, tail) / in.getRMSLevel (0, tail, tail));
        check (std::abs (levelDb) < 0.5f, "Drive at 0 % barely changes a quiet vocal", "level change " + db (levelDb));
    }
    {
        // Chorus: a centred (mono) vocal comes out different on the left and right.
        DiceVoxProcessor p;
        setParam (p, "chorus_on", 1.0f);
        setParam (p, ChorusModule::depthId, 60.0f);
        auto in  = sine (2, (int) sampleRate, 440.0, sampleRate, 0.3f);
        auto out = processAll (p, in, sampleRate);
        float sideMax = 0.0f;
        for (int i = (int) (0.5 * sampleRate); i < out.getNumSamples(); ++i)
            sideMax = std::max (sideMax, std::abs (out.getSample (0, i) - out.getSample (1, i)));
        check (allFinite (out) && sideMax > 0.01f && out.getMagnitude (0, out.getNumSamples()) < 0.6f,
               "Chorus spreads a centred vocal into stereo", "largest left/right difference " + std::to_string (sideMax));
    }
    {
        // Delay time follows the tempo.
        const bool ok = std::abs (DelayModule::delaySeconds (5, 120.0) - 0.5) < 1.0e-9    // 1/4 at 120 BPM
                     && std::abs (DelayModule::delaySeconds (3, 100.0) - 0.45) < 1.0e-9   // 1/8 dotted at 100 BPM
                     && std::abs (DelayModule::delaySeconds (5, 0.0) - 0.5) < 1.0e-9      // no tempo: 120 BPM
                     && DelayModule::delaySeconds (7, 30.0) <= DelayModule::maxDelaySeconds;
        check (ok && DelayModule::divisionNames().size() == 8, "Delay times are note divisions of the host tempo");
    }
    {
        // Delay: a single click comes back one quarter note (0.5 s at 120 BPM) later.
        DiceVoxProcessor p;
        setParam (p, "delay_on", 1.0f);
        setParam (p, DelayModule::timeId, 5.0f);
        setParam (p, DelayModule::feedbackId, 0.0f);
        setParam (p, DelayModule::mixId, 100.0f);
        juce::AudioBuffer<float> in (2, (int) sampleRate);
        in.clear();
        in.setSample (0, 0, 0.5f);
        in.setSample (1, 0, 0.5f);
        auto out = processAll (p, in, sampleRate);
        int echoAt = -1;
        float echoPeak = 0.0f;
        for (int i = 100; i < out.getNumSamples(); ++i)
            if (std::abs (out.getSample (0, i)) > echoPeak) { echoPeak = std::abs (out.getSample (0, i)); echoAt = i; }
        check (std::abs (echoAt - 24000) <= 2 && echoPeak > 0.2f, "Delay echo lands on the beat (1/4 at 120 BPM = 0.5 s)",
               "echo at sample " + std::to_string (echoAt) + " (expected 24000), level " + std::to_string (echoPeak));
    }
    {
        // Delay at maximum feedback with loud noise stays bounded (no runaway).
        DiceVoxProcessor p;
        setParam (p, "delay_on", 1.0f);
        setParam (p, DelayModule::timeId, 0.0f);
        setParam (p, DelayModule::feedbackId, DelayModule::maxFeedback);
        setParam (p, DelayModule::mixId, 100.0f);
        auto out = processAll (p, noise (2, (int) (6.0 * sampleRate), 0.5f, 17), sampleRate);
        const float peak = out.getMagnitude (0, out.getNumSamples());
        check (allFinite (out) && peak < 3.0f, "Delay at maximum feedback never runs away", "peak " + std::to_string (peak) + " for input peak 0.5");
    }
    {
        // Reverb: a click leaves a tail; at Mix 0 the vocal is untouched in level.
        DiceVoxProcessor p;
        setParam (p, "reverb_on", 1.0f);
        setParam (p, ReverbModule::sizeId, 80.0f);
        setParam (p, ReverbModule::mixId, 50.0f);
        juce::AudioBuffer<float> in (2, (int) (2.0 * sampleRate));
        in.clear();
        in.setSample (0, 0, 0.8f);
        in.setSample (1, 0, 0.8f);
        auto out = processAll (p, in, sampleRate);
        const float tailRms = out.getRMSLevel (0, (int) (0.3 * sampleRate), (int) (0.2 * sampleRate));

        DiceVoxProcessor q;
        setParam (q, "reverb_on", 1.0f);
        setParam (q, ReverbModule::mixId, 0.0f);
        const float dryDb = gainDb (q, 1000.0, sampleRate);
        check (allFinite (out) && tailRms > 1.0e-5f && std::abs (dryDb) < 0.5f, "Reverb adds a tail; Mix 0 leaves the vocal level alone",
               "tail RMS " + std::to_string (tailRms) + ", Mix 0 level " + db (dryDb));
    }
    {
        // Everything on at once, every setting jumping every block, block sizes
        // from 1 up to 1500 (prepared for 512): no NaN/Inf, no blow-up. Stereo and mono.
        for (int channels : { 2, 1 })
        {
            DiceVoxProcessor p;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (channels == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono());
            layout.outputBuses.add (channels == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono());
            p.setBusesLayout (layout);
            juce::Random rng (42 + channels);
            auto out = processAll (p, noise (channels, (int) (6.0 * sampleRate), 0.3f, 9), sampleRate, blockSize,
                                   [&] (int) { return 1 + rng.nextInt (1500); },
                                   [&] (int b)
                                   {
                                       for (auto* param : p.getParameters())
                                       {
                                           auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
                                           const auto id = ranged->getParameterID();
                                           if (id == ParamIDs::bypass || id == ParamIDs::inputGain || id == ParamIDs::outputGain)
                                               continue;
                                           if (id.endsWith ("_on"))
                                               ranged->setValueNotifyingHost ((b / 5) % 3 == 0 ? 0.0f : 1.0f);
                                           else
                                               ranged->setValueNotifyingHost (rng.nextFloat());
                                       }
                                   });
            const float peak = out.getMagnitude (0, out.getNumSamples());
            check (allFinite (out) && peak < 6.0f,
                   "All effects stay stable under extreme automation and odd block sizes (" + std::string (channels == 2 ? "stereo" : "mono") + ")",
                   "peak " + std::to_string (peak) + " for input peak 0.3");
        }
    }
    {
        // Same audio whatever the host's block sizes, with every effect on.
        auto in = noise (2, (int) sampleRate, 0.3f, 13);
        auto render = [&] (std::function<int (int)> sizes)
        {
            DiceVoxProcessor p;
            for (auto* id : allEffects)
                setParam (p, id, 1.0f);
            return processAll (p, in, sampleRate, blockSize, sizes);
        };
        juce::Random rng (8);
        auto fixedBlocks  = render ({});
        auto randomBlocks = render ([&] (int b) { return b == 0 ? 1500 : 1 + rng.nextInt (700); });
        float diff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < in.getNumSamples(); ++i)
                diff = std::max (diff, std::abs (fixedBlocks.getSample (ch, i) - randomBlocks.getSample (ch, i)));
        check (diff < 1.0e-4f, "Full chain output does not depend on host block size", "max diff " + std::to_string (diff));
    }
    {
        DiceVoxProcessor p;
        for (auto* id : allEffects)
            setParam (p, id, 1.0f);
        p.prepareToPlay (sampleRate, blockSize);
        check (p.getLatencySamples() == 0 && p.getTailLengthSeconds() >= 5.0,
               "Full chain adds no latency and reports a tail for reverb and echoes",
               std::to_string (p.getLatencySamples()) + " samples, tail " + std::to_string (p.getTailLengthSeconds()) + " s");
    }
    {
        // Dice: 300 rolls. Each switches on 2-4 effects, keeps every setting in
        // range, leaves Input / Mix / Output / Bypass alone and gets a name.
        // Over many rolls every effect comes up.
        DiceVoxProcessor p;
        setParam (p, ParamIDs::inputGain, 3.0f);
        setParam (p, ParamIDs::mix, 70.0f);
        juce::Random rng (2024);
        std::map<juce::String, int> timesOn;
        int badCount = 0, unnamed = 0;
        bool globalsKept = true, inRange = true;
        for (int r = 0; r < 300; ++r)
        {
            const auto name = p.rollDice (rng);
            int on = 0;
            for (auto* id : allEffects)
                if (p.state.getRawParameterValue (id)->load() > 0.5f) { ++on; ++timesOn[id]; }
            badCount += (on < 2 || on > 4) ? 1 : 0;
            unnamed  += name.isEmpty() || name != p.getRollName() ? 1 : 0;
            globalsKept &= std::abs (p.state.getRawParameterValue (ParamIDs::inputGain)->load() - 3.0f) < 0.05f
                        && std::abs (p.state.getRawParameterValue (ParamIDs::mix)->load() - 70.0f) < 0.05f
                        && p.state.getRawParameterValue (ParamIDs::bypass)->load() < 0.5f;
            for (auto* param : p.getParameters())
                inRange &= param->getValue() >= 0.0f && param->getValue() <= 1.0f;
        }
        bool everyEffect = true;
        std::string counts;
        for (auto* id : allEffects)
        {
            everyEffect &= timesOn[id] > 20;
            counts += std::string (id) + "=" + std::to_string (timesOn[id]) + " ";
        }
        check (badCount == 0 && unnamed == 0 && globalsKept && inRange && everyEffect,
               "ROLL: 2-4 effects every time, named, in range, never touches Input/Mix/Output/Bypass",
               "wrong count " + std::to_string (badCount) + ", unnamed " + std::to_string (unnamed) + ", " + counts);
    }
    {
        // Same seed, same roll (the dice are reproducible for testing).
        DiceVoxProcessor a, b;
        juce::Random ra (77), rb (77);
        const auto nameA = a.rollDice (ra), nameB = b.rollDice (rb);
        bool same = nameA == nameB;
        for (int i = 0; i < a.getParameters().size(); ++i)
            same &= std::abs (a.getParameters()[i]->getValue() - b.getParameters()[i]->getValue()) < 1.0e-6f;
        check (same, "Same dice seed gives the same roll", (nameA + " / " + nameB).toStdString());
    }
    {
        // Rolled chains sound sane: 60 random rolls on noise, across every style
        // and intensity, all finite and bounded.
        float worst = 0.0f;
        bool finite = true;
        for (int seed = 1; seed <= 60; ++seed)
        {
            DiceVoxProcessor p;
            setParam (p, ParamIDs::diceStyle, (float) (seed % 6));
            setParam (p, ParamIDs::diceIntensity, (float) ((seed * 37) % 101));
            juce::Random rng (seed);
            p.rollDice (rng);
            auto out = processAll (p, noise (2, (int) (1.5 * sampleRate), 0.3f, seed), sampleRate);
            finite &= allFinite (out);
            worst = std::max (worst, out.getMagnitude (0, out.getNumSamples()));
        }
        check (finite && worst < 2.5f, "60 random rolls (all styles and intensities) play back cleanly (finite, bounded)",
               "loudest peak " + std::to_string (worst) + " for input peak 0.3");
    }
    {
        // BACK: roll A, roll B, BACK -> exactly A again (settings and name).
        DiceVoxProcessor p;
        juce::Random rng (5);
        const auto nameA = p.rollDice (rng);
        std::vector<float> afterA;
        for (auto* param : p.getParameters())
            afterA.push_back (param->getValue());
        p.rollDice (rng);
        const bool undone = p.undoRoll();
        bool same = undone && p.getRollName() == nameA;
        for (int i = 0; i < p.getParameters().size(); ++i)
            same &= std::abs (p.getParameters()[i]->getValue() - afterA[(size_t) i]) < 1.0e-6f;

        // Back past the first roll: everything off again; then nothing left to undo.
        same &= p.undoRoll() && ! p.canUndoRoll() && ! p.undoRoll();
        for (auto* id : allEffects)
            same &= p.state.getRawParameterValue (id)->load() < 0.5f;
        check (same, "BACK restores the previous roll exactly, then the starting sound", nameA.toStdString());
    }
    {
        // The roll's name is saved with the project.
        juce::MemoryBlock saved;
        juce::String name;
        {
            DiceVoxProcessor p;
            juce::Random rng (11);
            name = p.rollDice (rng);
            p.getStateInformation (saved);
        }
        DiceVoxProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        check (restored.getRollName() == name && ! restored.canUndoRoll(), "Roll name survives save/reload",
               (name + " -> " + restored.getRollName()).toStdString());
    }

    // ---------------------------------------------------------------------
    // v2: pitch, lo-fi, chop, reverse, and the dice controls
    // ---------------------------------------------------------------------
    auto valueOf = [] (DiceVoxProcessor& p, const juce::String& id) { return p.state.getRawParameterValue (id)->load(); };
    {
        // Pitch: the loudest frequency moves by the set number of semitones.
        auto dominantHz = [] (const juce::AudioBuffer<float>& b, int start)
        {
            constexpr int order = 15, size = 1 << order;
            std::vector<float> data (2 * size, 0.0f);
            std::copy (b.getReadPointer (0) + start, b.getReadPointer (0) + start + size, data.begin());
            juce::dsp::WindowingFunction<float> (size, juce::dsp::WindowingFunction<float>::hann)
                .multiplyWithWindowingTable (data.data(), size);
            juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (data.data());
            const auto peak = std::max_element (data.begin() + 1, data.begin() + size / 2);
            return (float) ((double) std::distance (data.begin(), peak) * sampleRate / size);
        };
        bool ok = true;
        std::string detail;
        for (float shift : { 12.0f, -12.0f, 7.0f })
        {
            DiceVoxProcessor p;
            setParam (p, "pitch_on", 1.0f);
            setParam (p, PitchModule::shiftId, shift);
            setParam (p, PitchModule::mixId, 100.0f);
            auto out = processAll (p, sine (2, (int) (2.0 * sampleRate), 300.0, sampleRate, 0.4f), sampleRate);
            const float hz = dominantHz (out, (int) (0.5 * sampleRate));
            const float expected = 300.0f * std::pow (2.0f, shift / 12.0f);
            ok &= allFinite (out) && std::abs (hz - expected) < 0.04f * expected;
            detail += std::to_string ((int) shift) + " st: " + std::to_string ((int) hz) + " Hz (expected "
                    + std::to_string ((int) expected) + ")  ";
        }
        check (ok, "Pitch shifts the vocal by the set number of semitones", detail);
    }
    {
        DiceVoxProcessor p;
        setParam (p, "pitch_on", 1.0f);
        setParam (p, PitchModule::mixId, 0.0f);
        const auto diff = run (p).maxDiff;
        check (diff < 1.0e-5f, "Pitch at Mix 0 leaves the vocal untouched", "max diff " + std::to_string (diff));
    }
    {
        // Lo-Fi: at Mix 0 / Wobble 0 nothing changes; fully crushed the sound is
        // clearly degraded but not louder.
        DiceVoxProcessor clean;
        setParam (clean, "lofi_on", 1.0f);
        setParam (clean, LoFiModule::mixId, 0.0f);
        setParam (clean, LoFiModule::wobbleId, 0.0f);
        const auto cleanDiff = run (clean).maxDiff;

        DiceVoxProcessor p;
        setParam (p, "lofi_on", 1.0f);
        setParam (p, LoFiModule::crushId, 100.0f);
        setParam (p, LoFiModule::wobbleId, 0.0f);
        setParam (p, LoFiModule::mixId, 100.0f);
        auto in  = sine (2, (int) sampleRate, 1000.0, sampleRate, 0.4f);
        auto out = processAll (p, in, sampleRate);
        const int tail = (int) (0.5 * sampleRate);
        float diff = 0.0f;
        for (int i = tail; i < out.getNumSamples(); ++i)
            diff = std::max (diff, std::abs (out.getSample (0, i) - in.getSample (0, i)));
        const float levelDb = juce::Decibels::gainToDecibels (out.getRMSLevel (0, tail, tail) / in.getRMSLevel (0, tail, tail));
        check (cleanDiff < 1.0e-5f && allFinite (out) && diff > 0.05f && levelDb < 1.0f && levelDb > -12.0f,
               "Lo-Fi crushes audibly without getting louder; Mix 0 + Wobble 0 is untouched",
               "clean diff " + std::to_string (cleanDiff) + ", crushed diff " + std::to_string (diff) + ", level " + db (levelDb));
    }
    {
        // Lo-Fi wobble sways the pitch but stays bounded and click-free.
        DiceVoxProcessor p;
        setParam (p, "lofi_on", 1.0f);
        setParam (p, LoFiModule::wobbleId, 100.0f);
        setParam (p, LoFiModule::mixId, 0.0f);
        auto in  = sine (2, (int) (2.0 * sampleRate), 440.0, sampleRate, 0.4f);
        auto out = processAll (p, in, sampleRate);
        check (allFinite (out) && out.getMagnitude (0, out.getNumSamples()) < 0.45f && maxStep (out) < 1.25f * maxStep (in),
               "Lo-Fi wobble stays bounded and click-free");
    }
    {
        // Chop: 1/8 notes at 120 BPM = 8 gaps in 2 s, silent at Depth 100, click-free edges.
        DiceVoxProcessor p;
        setParam (p, "chop_on", 1.0f);
        setParam (p, ChopModule::rateId, 3.0f); // 1/8
        setParam (p, ChopModule::depthId, 100.0f);
        auto in  = sine (2, (int) (2.0 * sampleRate), 1000.0, sampleRate, 0.4f);
        auto out = processAll (p, in, sampleRate);
        const int win = 240;
        int gaps = 0;
        bool open = true;
        float quietest = 1.0f;
        for (int start = 0; start + win <= out.getNumSamples(); start += win)
        {
            const float rms = out.getRMSLevel (0, start, win);
            quietest = std::min (quietest, rms);
            if (open && rms < 0.02f)       { ++gaps; open = false; }
            else if (! open && rms > 0.2f) { open = true; }
        }
        check (allFinite (out) && gaps == 8 && quietest < 0.001f && maxStep (out) < 1.25f * maxStep (in),
               "Chop gates in time with the tempo (1/8 at 120 BPM), silent gaps, no clicks",
               std::to_string (gaps) + " gaps (expected 8), quietest " + std::to_string (quietest));
    }
    {
        // Reverse: adds reversed slices on top of the vocal; Mix 0 leaves it alone.
        DiceVoxProcessor p;
        setParam (p, "reverse_on", 1.0f);
        setParam (p, ReverseModule::mixId, 100.0f);
        auto in  = sine (2, (int) (3.0 * sampleRate), 440.0, sampleRate, 0.3f);
        auto out = processAll (p, in, sampleRate);
        double wetSum = 0.0;
        const int from = (int) sampleRate;
        for (int i = from; i < out.getNumSamples(); ++i)
            wetSum += juce::square ((double) (out.getSample (0, i) - in.getSample (0, i)));
        const float wetRms = (float) std::sqrt (wetSum / (out.getNumSamples() - from));

        DiceVoxProcessor q;
        setParam (q, "reverse_on", 1.0f);
        setParam (q, ReverseModule::mixId, 0.0f);
        const auto dryDiff = run (q).maxDiff;

        check (allFinite (out) && wetRms > 0.05f && out.getMagnitude (0, out.getNumSamples()) < 1.0f && dryDiff < 1.0e-5f
                   && std::abs (ReverseModule::sliceSeconds (2, 120.0) - 0.5) < 1.0e-9,
               "Reverse adds tempo-length reversed slices; Mix 0 leaves the vocal alone",
               "reversed RMS " + std::to_string (wetRms) + ", Mix 0 diff " + std::to_string (dryDiff));
    }
    {
        // Locks: a locked effect keeps its on/off state and settings through 50
        // rolls, even when the style would want it; the dice controls themselves
        // never change (not by ROLL, not by BACK).
        DiceVoxProcessor p;
        setParam (p, "reverb_on", 1.0f);
        setParam (p, ReverbModule::sizeId, 33.0f);
        setParam (p, ParamIDs::diceLock ("reverb"), 1.0f);
        setParam (p, ParamIDs::diceLock ("chop"), 1.0f); // locked off
        setParam (p, ParamIDs::diceIntensity, 80.0f);
        setParam (p, ParamIDs::diceStyle, 5.0f);         // Glitch, whose signature effect is Chop
        auto controlsKept = [&]
        {
            return std::abs (valueOf (p, ParamIDs::diceIntensity) - 80.0f) < 0.5f && std::abs (valueOf (p, ParamIDs::diceStyle) - 5.0f) < 0.5f
                && valueOf (p, ParamIDs::diceLock ("reverb")) > 0.5f && valueOf (p, ParamIDs::diceLock ("chop")) > 0.5f;
        };
        juce::Random rng (3);
        bool kept = true;
        for (int r = 0; r < 50; ++r)
        {
            p.rollDice (rng);
            kept &= valueOf (p, "reverb_on") > 0.5f && std::abs (valueOf (p, ReverbModule::sizeId) - 33.0f) < 0.05f
                 && valueOf (p, "chop_on") < 0.5f && controlsKept();
        }
        p.undoRoll();
        kept &= controlsKept();
        check (kept, "Locked effects survive every roll; ROLL and BACK never change the dice controls");
    }
    {
        // Intensity: 0 % rolls 1-3 effects, 100 % rolls 3-5.
        auto stats = [&] (float intensity)
        {
            DiceVoxProcessor p;
            setParam (p, ParamIDs::diceIntensity, intensity);
            juce::Random rng (99);
            int lo = 99, hi = 0;
            double sum = 0.0;
            for (int r = 0; r < 200; ++r)
            {
                p.rollDice (rng);
                int on = 0;
                for (auto* id : allEffects)
                    on += valueOf (p, id) > 0.5f ? 1 : 0;
                lo = std::min (lo, on);
                hi = std::max (hi, on);
                sum += on;
            }
            return std::make_tuple (lo, hi, sum / 200.0);
        };
        const auto [lowMin, lowMax, lowAvg] = stats (0.0f);
        const auto [highMin, highMax, highAvg] = stats (100.0f);
        check (lowMin >= 1 && lowMax <= 3 && highMin >= 3 && highMax <= 5 && highAvg > lowAvg + 1.5,
               "Intensity sets how many effects a roll uses (low 1-3, high 3-5)",
               "low " + std::to_string (lowMin) + "-" + std::to_string (lowMax) + " (avg " + std::to_string (lowAvg) + "), high "
                   + std::to_string (highMin) + "-" + std::to_string (highMax) + " (avg " + std::to_string (highAvg) + ")");
    }
    {
        // Every style always brings its signature effect.
        struct Signature { int style; const char* effectOn; };
        bool ok = DiceRoller::styleNames().size() == 6;
        std::string detail;
        for (auto s : { Signature { 1, "reverb_on" }, Signature { 2, "lofi_on" }, Signature { 3, "pitch_on" },
                        Signature { 4, "lofi_on" }, Signature { 5, "chop_on" } })
        {
            DiceVoxProcessor p;
            setParam (p, ParamIDs::diceStyle, (float) s.style);
            juce::Random rng (s.style * 7);
            int misses = 0;
            for (int r = 0; r < 40; ++r)
            {
                p.rollDice (rng);
                misses += valueOf (p, s.effectOn) < 0.5f ? 1 : 0;
            }
            ok &= misses == 0;
            detail += DiceRoller::styleNames()[s.style].toStdString() + ": " + std::to_string (misses) + " misses  ";
        }
        check (ok, "Each dice style always includes its signature effect", detail);
    }

    // ---------------------------------------------------------------------
    // Presets
    // ---------------------------------------------------------------------
    auto tempPresetFolder = []
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("DiceVoxPresetTest-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    };
    {
        // Save the whole sound, change everything, load it back exactly. A preset
        // never touches Bypass or the dice controls, and BACK undoes a load.
        const auto folder = tempPresetFolder();
        DiceVoxProcessor p;
        p.presets.setFolder (folder);
        juce::Random rng (21);
        p.rollDice (rng);
        setParam (p, ParamIDs::outputGain, -4.0f);
        setParam (p, ParamIDs::diceIntensity, 90.0f);
        const auto rolled = p.getRollName();
        std::vector<float> savedValues;
        for (auto* param : p.getParameters())
            savedValues.push_back (param->getValue());

        const auto file = p.savePreset ("My: Dreamy/Hook?"); // characters Windows can't use in names
        bool ok = file.existsAsFile() && file.getParentDirectory() == folder && file.hasFileExtension ("dicevox")
               && p.getPresetName() == file.getFileNameWithoutExtension();

        juce::Random other (22);
        p.rollDice (other);
        setParam (p, ParamIDs::outputGain, 6.0f);
        setParam (p, ParamIDs::bypass, 1.0f);
        ok &= p.loadPreset (file);

        std::string mismatch;
        for (int i = 0; i < p.getParameters().size(); ++i)
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[i]);
            const auto id = ranged->getParameterID();
            if (id == ParamIDs::bypass || ParamIDs::isDiceControl (id))
                continue;
            if (std::abs (ranged->getValue() - savedValues[(size_t) i]) > 1.0e-4f)
                mismatch += id.toStdString() + " ";
        }
        ok &= mismatch.empty() && p.getRollName() == rolled
           && valueOf (p, ParamIDs::bypass) > 0.5f
           && std::abs (valueOf (p, ParamIDs::diceIntensity) - 90.0f) < 0.5f;

        ok &= p.undoRoll() && std::abs (valueOf (p, ParamIDs::outputGain) - 6.0f) < 0.05f && p.getPresetName().isEmpty();
        check (ok, "Presets save the whole sound and load it back exactly; BACK undoes a load",
               "file " + file.getFileName().toStdString() + ", mismatched: " + mismatch);
        folder.deleteRecursively();
    }
    {
        // The preset list is sorted and only shows presets; a broken file is
        // refused; an older preset loads with defaults for what it doesn't have.
        const auto folder = tempPresetFolder();
        folder.createDirectory();
        const juce::String oldPreset = R"(<DiceVoxPreset formatVersion="1" rollName="OLD ONE">)"
                                       R"(<PARAM id="reverb_on" value="1"/><PARAM id="reverb_mix" value="44"/></DiceVoxPreset>)";
        folder.getChildFile ("b old.dicevox").replaceWithText (oldPreset);
        folder.getChildFile ("A first.dicevox").replaceWithText (oldPreset);
        folder.getChildFile ("notes.txt").replaceWithText ("not a preset");
        folder.getChildFile ("broken.dicevox").replaceWithText ("this is not xml");

        DiceVoxProcessor p;
        p.presets.setFolder (folder);
        setParam (p, "delay_on", 1.0f);
        setParam (p, DelayModule::mixId, 77.0f);
        setParam (p, ParamIDs::mix, 50.0f);

        const auto files = p.presets.list();
        bool ok = files.size() == 3 && files[0].getFileName() == "A first.dicevox" && files[1].getFileName() == "b old.dicevox";
        ok &= ! p.loadPreset (folder.getChildFile ("broken.dicevox")) && ! p.canUndoRoll();
        ok &= p.loadPreset (files[1]);
        ok &= valueOf (p, "reverb_on") > 0.5f && std::abs (valueOf (p, ReverbModule::mixId) - 44.0f) < 0.05f
           && valueOf (p, "delay_on") < 0.5f && std::abs (valueOf (p, DelayModule::mixId) - 30.0f) < 0.05f
           && std::abs (valueOf (p, ParamIDs::mix) - 100.0f) < 0.05f
           && p.getPresetName() == "b old" && p.getRollName() == "OLD ONE";
        check (ok, "Preset list is sorted and filtered; broken files are refused; older presets fill in defaults",
               std::to_string (files.size()) + " presets listed");
        folder.deleteRecursively();
    }
    {
        // The preset bar's < > step through the presets and wrap around.
        const auto folder = tempPresetFolder();
        DiceVoxProcessor p;
        p.presets.setFolder (folder);
        setParam (p, "reverb_on", 1.0f);
        p.savePreset ("One");
        setParam (p, "delay_on", 1.0f);
        p.savePreset ("Two");

        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        std::function<void (juce::Component&, std::function<void (juce::Component&)>)> visit =
            [&] (juce::Component& c, std::function<void (juce::Component&)> fn) { fn (c); for (auto* ch : c.getChildren()) visit (*ch, fn); };
        PresetBar* bar = nullptr;
        visit (*editor, [&] (juce::Component& c) { if (auto* b = dynamic_cast<PresetBar*> (&c)) bar = b; });

        std::string seen;
        if (bar != nullptr)
            for (int i = 0; i < 3; ++i)
            {
                bar->step (1);
                seen += p.getPresetName().toStdString() + " ";
            }
        check (bar != nullptr && seen == "One Two One " && valueOf (p, "delay_on") < 0.5f,
               "Preset bar steps through presets (and loads them)", seen);
        folder.deleteRecursively();
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
