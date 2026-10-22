#include "PresetBar.h"
#include "PluginProcessor.h"

using namespace ahe::theme;

namespace
{
    constexpr int saveAsNewItem  = 10001;
    constexpr int openFolderItem = 10002;
}

PresetBar::PresetBar (DiceVoxProcessor& p)
    : processor (p)
{
    previousButton.setTooltip ("Previous preset");
    previousButton.setTitle ("Previous preset");
    previousButton.onClick = [this] { step (-1); };

    nextButton.setTooltip ("Next preset");
    nextButton.setTitle ("Next preset");
    nextButton.onClick = [this] { step (1); };

    nameButton.setTooltip ("Your presets: click to load one, save a new one or open the presets folder.");
    nameButton.setTitle ("Presets");
    nameButton.onClick = [this] { showMenu(); };

    saveButton.setTooltip ("SAVE: store the current sound as a preset you can load in any project.");
    saveButton.setTitle ("Save preset");
    saveButton.onClick = [this] { askForName(); };

    for (auto* b : { &previousButton, &nameButton, &nextButton, &saveButton })
    {
        b->setWantsKeyboardFocus (true);
        addAndMakeVisible (*b);
    }

    refresh();
    startTimerHz (4);
}

PresetBar::~PresetBar()
{
    // Close an open SAVE dialog; its callback sees this bar is gone and does nothing.
    if (auto* d = dialog.getComponent())
        d->exitModalState (0);
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    previousButton.setBounds (area.removeFromLeft (30));
    area.removeFromLeft (space::xs);
    saveButton.setBounds (area.removeFromRight (64));
    area.removeFromRight (space::xs);
    nextButton.setBounds (area.removeFromRight (30));
    area.removeFromRight (space::xs);
    nameButton.setBounds (area);
}

void PresetBar::timerCallback()
{
    refresh();
}

void PresetBar::refresh()
{
    const auto name = processor.getPresetName();
    const auto text = name.isEmpty() ? juce::String (noPresetText) : name;
    if (nameButton.getButtonText() != text)
        nameButton.setButtonText (text);
}

void PresetBar::step (int offset)
{
    const auto files = processor.presets.list();
    if (files.isEmpty())
        return;

    const auto current = processor.getPresetName();
    int index = -1;
    for (int i = 0; i < files.size(); ++i)
        if (PresetStore::nameOf (files[i]) == current)
            index = i;

    const int count = files.size();
    const int next = index < 0 ? (offset > 0 ? 0 : count - 1)
                               : ((index + offset) % count + count) % count;
    processor.loadPreset (files[next]);
    refresh();
}

bool PresetBar::saveAs (const juce::String& name)
{
    const bool ok = processor.savePreset (name).existsAsFile();
    refresh();
    return ok;
}

void PresetBar::showMenu()
{
    const auto files = processor.presets.list();
    const auto current = processor.getPresetName();

    juce::PopupMenu menu;
    if (files.isEmpty())
        menu.addItem (-1, "No presets yet: click SAVE to make one", false);

    for (int i = 0; i < files.size(); ++i)
    {
        const auto name = PresetStore::nameOf (files[i]);
        menu.addItem (i + 1, name, true, name == current);
    }

    menu.addSeparator();
    menu.addItem (saveAsNewItem, "Save as new preset...");
    menu.addItem (openFolderItem, "Open presets folder");

    juce::Component::SafePointer<PresetBar> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                        [safeThis, files] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            if (result == saveAsNewItem)
                            {
                                safeThis->askForName();
                            }
                            else if (result == openFolderItem)
                            {
                                const auto folder = safeThis->processor.presets.getFolder();
                                folder.createDirectory();
                                folder.startAsProcess();
                            }
                            else if (result >= 1 && result <= files.size())
                            {
                                safeThis->processor.loadPreset (files[result - 1]);
                                safeThis->refresh();
                            }
                        });
}

void PresetBar::askForName()
{
    if (dialog != nullptr)
        return;

    auto suggestion = processor.getPresetName();
    if (suggestion.isEmpty())
        suggestion = processor.getRollName();
    if (suggestion.isEmpty())
        suggestion = "My DiceVox Sound";

    auto* window = new juce::AlertWindow ("Save preset",
                                          "Name this sound. You can load it from the preset menu in any project. "
                                          "Using an existing name replaces that preset.",
                                          juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", suggestion, "Preset name");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog = window;

    juce::Component::SafePointer<PresetBar> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        // Runs before the window is deleted, so its text is still readable.
        if (safeThis == nullptr || result != 1)
            return;

        const auto name = window->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            safeThis->saveAs (name);
    }), true);
}
