#pragma once

#include <JuceHeader.h>
#include "Theme.h"
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
        save.setBounds(b.removeFromRight(58));
        b.removeFromRight(8);
        previous.setBounds(b.removeFromLeft(28));
        next.setBounds(b.removeFromRight(28));
        nameButton.setBounds(b.reduced(4, 0));
    }

private:
    /// Pill showing the current preset name with a drop-down caret.
    class NameButton : public juce::Button
    {
    public:
        NameButton() : juce::Button({}) {}

        void paintButton(juce::Graphics& g, bool over, bool down) override
        {
            const auto area = getLocalBounds().toFloat().reduced(0.5f);
            const float corner = area.getHeight() * 0.5f;
            g.setColour(Theme::glass.brighter(down ? 0.15f : (over ? 0.08f : 0.0f)));
            g.fillRoundedRectangle(area, corner);
            g.setColour(over ? Theme::accent.withAlpha(0.5f) : juce::Colours::white.withAlpha(0.08f));
            g.drawRoundedRectangle(area, corner, 1.0f);

            g.setColour(Theme::accent.brighter(0.4f));
            g.setFont(Theme::font(13.5f));
            g.drawText(getButtonText(), area.withTrimmedLeft(14.0f).withTrimmedRight(28.0f),
                       juce::Justification::centredLeft, true);

            juce::Path caret;
            const float cx = area.getRight() - 16.0f, cy = area.getCentreY();
            caret.startNewSubPath(cx - 4.0f, cy - 2.0f);
            caret.lineTo(cx, cy + 2.0f);
            caret.lineTo(cx + 4.0f, cy - 2.0f);
            g.strokePath(caret, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
    juce::TextButton previous { "<" }, next { ">" }, save { "SAVE" };
};

} // namespace AxisEQ
