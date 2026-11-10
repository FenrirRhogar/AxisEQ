#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "RackUI.h"
#include "Utilities/PresetManager.h"

namespace AxisEQ
{

/// Preset selector: previous / name / next, plus save. The menu is built from scratch each
/// time it opens, so it is always current and nothing rebuilds it while it is on screen.
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar(PresetManager& manager) : presets(manager)
    {
        previous.onClick = [this] { presets.loadPreviousPreset(); refresh(); };
        next.onClick     = [this] { presets.loadNextPreset(); refresh(); };
        save.onClick     = [this] { if (onSaveRequested) onSaveRequested(); };
        nameButton.onClick = [this] { showMenu(); };
        for (auto* c : std::initializer_list<juce::Component*> { &previous, &next, &save, &nameButton })
            addAndMakeVisible(c);
        refresh();
        startTimerHz(5);   // follows program changes made by the host
    }

    /// Opens the editor's save dialog.
    std::function<void()> onSaveRequested;

    void refresh()
    {
        const int index = presets.getCurrentIndex();
        const auto name = index >= 0 && index < presets.getNumPresets() ? presets.getPreset(index).name : juce::String();
        if (name != nameButton.getButtonText())
            nameButton.setButtonText(name);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        save.setBounds(b.removeFromRight(58).withSizeKeepingCentre(58, 22));
        b.removeFromRight(8);
        previous.setBounds(b.removeFromLeft(30).withSizeKeepingCentre(30, 22));
        b.removeFromLeft(6);
        next.setBounds(b.removeFromRight(30).withSizeKeepingCentre(30, 22));
        b.removeFromRight(6);
        nameButton.setBounds(b);
    }

private:
    // The preset's name in a recessed OLED window; its category on the right. Click for the menu.
    class NameButton : public juce::Button
    {
    public:
        NameButton() : juce::Button({}) {}

        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            const auto area = getLocalBounds().toFloat();
            rackui::oled(g, area);
            rackui::oledText(g, getButtonText().toUpperCase(), area.reduced(12.0f, 0.0f).withTrimmedRight(14.0f), Theme::accent, 12.5f);
            juce::Path caret;
            const float cx = area.getRight() - 12.0f, cy = area.getCentreY();
            caret.startNewSubPath(cx - 3.5f, cy - 1.5f);
            caret.lineTo(cx, cy + 2.0f);
            caret.lineTo(cx + 3.5f, cy - 1.5f);
            g.setColour(Theme::accent.withAlpha(over ? 0.9f : 0.45f));
            g.strokePath(caret, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    void timerCallback() override { refresh(); }

    void showMenu()
    {
        presets.scanUserPresetsFromDisk();
        const int current = presets.getCurrentIndex();

        juce::PopupMenu menu;
        juce::String category;
        for (int i = 0; i < presets.getNumPresets(); ++i)
        {
            const auto& p = presets.getPreset(i);
            const auto heading = i < presets.getNumFactoryPresets() ? p.category : juce::String("User");
            if (heading != category)
            {
                menu.addSectionHeader(heading);
                category = heading;
            }
            menu.addItem(p.name, true, i == current, [this, i] { presets.loadPreset(i); refresh(); });
        }

        menu.addSeparator();
        menu.addItem("Save preset...", [this] { if (onSaveRequested) onSaveRequested(); });
        menu.addItem("Reset to Flat", [this] { presets.loadPreset(0); refresh(); });
        if (current >= presets.getNumFactoryPresets())
            menu.addItem("Delete \"" + presets.getPreset(current).name + "\"",
                         [this, current] { presets.deleteUserPreset(current); refresh(); });
        menu.addItem("Open preset folder", [this] { presets.getUserPresetsDirectory().startAsProcess(); });

        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(&nameButton)
                               .withMinimumWidth(nameButton.getWidth())
                               .withMaximumNumColumns(1));
    }

    PresetManager& presets;
    NameButton nameButton;
    rackui::CapButton previous { "<" }, next { ">" }, save { "STORE" };
};

} // namespace AxisEQ
