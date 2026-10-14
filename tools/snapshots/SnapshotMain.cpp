// DiceVoxSnapshots — renders the plug-in editor to PNG files for design review.
//
//   cmake -B build -DDICEVOX_BUILD_SNAPSHOTS=ON
//   cmake --build build --target DiceVoxSnapshots
//   ./build/DiceVoxSnapshots <output-folder>
//
// Runs as a small GUI app with a real event loop, so keyboard focus, hover and
// tooltips are captured as they really behave (nothing is faked in the paint
// code). It only talks to the editor through parameter IDs and the component
// tree, so the same tool works on any version of the UI (before/after shots).
// On Linux, run it under a display with a window manager, e.g. xvfb + openbox.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"

#include <iostream>

namespace
{
    void setParam (DiceVoxProcessor& p, const juce::String& id, float realValue)
    {
        auto* param = p.state.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }

    void resetParams (DiceVoxProcessor& p)
    {
        for (auto* param : p.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    template <typename T>
    T* findFirst (juce::Component& root, std::function<bool (T&)> pred = nullptr)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* hit = dynamic_cast<T*> (child))
                if (pred == nullptr || pred (*hit))
                    return hit;
            if (auto* hit = findFirst<T> (*child, pred))
                return hit;
        }
        return nullptr;
    }

    juce::Component* findTooltipClient (juce::Component& root, const juce::String& textFragment)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* ttc = dynamic_cast<juce::TooltipClient*> (child))
                if (ttc->getTooltip().contains (textFragment))
                    return child;
            if (auto* hit = findTooltipClient (*child, textFragment))
                return hit;
        }
        return nullptr;
    }
}

// Plug-in code can't use START_JUCE_APPLICATION, so this drives JUCE's message
// loop directly from main() and steps through the states with a Timer.
class SnapshotRunner : private juce::Timer
{
public:
    int failures = 0;

    void start (const juce::String& outputArg)
    {
        auto arg = outputArg.trim().unquoted();
        outDir = juce::File::getCurrentWorkingDirectory().getChildFile (arg.isNotEmpty() ? arg : "snapshots");
        outDir.createDirectory();
        std::cout << "DiceVox UI snapshots -> " << outDir.getFullPathName() << "\n";

        editor.reset (processor.createEditor());
        baseW = editor->getWidth();
        baseH = editor->getHeight();
        editor->setTopLeftPosition (100, 100);
        editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
        editor->setVisible (true);
        editor->toFront (true);

        startTimer (400);
    }

    void finish()
    {
        if (editor != nullptr)
            editor->removeFromDesktop();
        editor.reset();
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }

private:
    void timerCallback() override
    {
        // Each step sets up a state, then the next tick (after repaint/hover
        // timers have run) captures it.
        switch (step++)
        {
            case 0:  resetParams (processor); break;
            case 1:  save ("01-default.png"); break;

            case 2:
                setParam (processor, ParamIDs::inputGain, -9.4f);
                setParam (processor, ParamIDs::mix, 40.0f);
                setParam (processor, ParamIDs::outputGain, 8.4f);
                setParam (processor, ParamIDs::bypass, 1.0f);
                break;
            case 3:  save ("02-adjusted-bypassed.png"); break;

            case 4:
                // Real keyboard navigation: focus the first control, then press Tab
                // (focus rings are designed to appear only during keyboard use).
                resetParams (processor);
                editor->toFront (true);
                if (auto* first = findFirst<juce::Component> (*editor, [] (juce::Component& c) { return c.getWantsKeyboardFocus() && c.isEnabled(); }))
                    first->grabKeyboardFocus();
                pressKey ("Tab");
                break;
            case 5:
                if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
                    std::cout << "  focused: " << focused->getTitle() << "\n";
                else
                    std::cout << "  (no component has keyboard focus; focus ring not captured)\n";
                save ("03-keyboard-focus.png");
                juce::Component::unfocusAllComponents();
                break;

            case 6:
                // Hover the ROLL chamber and wait for its tooltip.
                if (auto* roll = findTooltipClient (*editor, "ROLL is not available"))
                    movePointer (roll->getScreenBounds().getCentre());
                stopTimer();
                startTimer (2500);
                break;
            case 7:
                saveWithTooltip ("04-hover-roll-tooltip.png");
                movePointer ({ 1500, 900 });
                stopTimer();
                startTimer (400);
                break;

            case 8:
                if (editor->isResizable())
                    editor->setSize (juce::roundToInt (baseW * 1.5), juce::roundToInt (baseH * 1.5));
                break;
            case 9:
                if (editor->isResizable()) save ("05-large-150pct.png");
                else std::cout << "  (editor is fixed-size; no large-window shot)\n";
                editor->setSize (baseW, baseH);
                break;

            case 10: break;
            case 11: save ("06-default-hidpi-2x.png", 2.0f); break;

            default:
                stopTimer();
                finish();
                break;
        }
    }

    // Real pointer motion (X11 needs genuine motion events for hover/tooltips).
    static void movePointer (juce::Point<int> screenPos)
    {
       #if JUCE_LINUX
        juce::ChildProcess xdotool;
        if (xdotool.start ("xdotool mousemove " + juce::String (screenPos.x) + " " + juce::String (screenPos.y)))
        {
            xdotool.waitForProcessToFinish (2000);
            return;
        }
       #endif
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition (screenPos.toFloat());
    }

    static void pressKey (const juce::String& key)
    {
       #if JUCE_LINUX
        juce::ChildProcess xdotool;
        if (xdotool.start ("xdotool key " + key))
            xdotool.waitForProcessToFinish (2000);
       #else
        juce::ignoreUnused (key);
       #endif
    }

    juce::Image capture (float scale)
    {
        return editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    }

    void write (const juce::Image& image, const juce::String& name)
    {
        auto file = outDir.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream out (file);
        const bool ok = out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << (ok ? "  wrote " : "  FAILED ") << name << "  (" << image.getWidth() << "x" << image.getHeight() << ")\n";
        failures += ok ? 0 : 1;
    }

    void save (const juce::String& name, float scale = 1.0f)
    {
        write (capture (scale), name);
    }

    // The editor owns its TooltipWindow as a child component, so a visible
    // tooltip is already part of the editor snapshot.
    void saveWithTooltip (const juce::String& name)
    {
        auto* tip = findFirst<juce::TooltipWindow> (*editor);
        std::cout << ((tip != nullptr && tip->isVisible()) ? "  tooltip visible\n" : "  (no tooltip visible)\n");
        save (name);
    }

    DiceVoxProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File outDir;
    int baseW = 0, baseH = 0, step = 0;
};

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    SnapshotRunner runner;
    runner.start (argc > 1 ? juce::String (argv[1]) : juce::String());
    juce::MessageManager::getInstance()->runDispatchLoop();
    return runner.failures == 0 ? 0 : 1;
}
