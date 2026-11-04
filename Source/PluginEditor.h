#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/EQCurveComponent.h"
#include "GUI/ControlPanelComponent.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/CustomLookAndFeel.h"
#include "Utilities/PresetManager.h"

//==============================================================================
/// AxisEQ top-level editor. Owns:
///   - TopBarComponent  (logo | preset browser | output gain | bypass)
///   - EQCurveComponent (spectrum + interactive curve + nodes)
///   - ControlPanelComponent (per-band strip, docked at the bottom)
class AxisEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AxisEQAudioProcessorEditor(AxisEQAudioProcessor&);
    ~AxisEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AxisEQAudioProcessor& processorRef;

    // ── Preset system ────────────────────────────────────────────────────
    AxisEQ::PresetManager* presetManager = nullptr;   // owned by the processor

    // ── Spectrum thread ──────────────────────────────────────────────────
    std::unique_ptr<AxisEQ::SpectrumAnalyzer> analyzer;
    std::unique_ptr<AxisEQ::EQCurveComponent> eqCurve;
    std::unique_ptr<AxisEQ::ControlPanelComponent> controlPanel;

    AxisEQ::CustomLookAndFeel customLookAndFeel;

    // ── Top-bar UI components ─────────────────────────────────────────────
    juce::ComboBox presetCombo;
    juce::TextButton prevPresetBtn { "<" };
    juce::TextButton nextPresetBtn { ">" };
    juce::TextButton savePresetBtn { "Save" };
    juce::TextButton resetBtn      { "Reset" };

    // ── Output gain slider ────────────────────────────────────────────────
    // (future expansion — placeholder style only for now)

    void buildTopBar();
    void layoutTopBar(juce::Rectangle<int>& area);

    void populatePresetCombo();
    void onPresetComboChanged();
    void onSavePreset();
    void onResetPreset();

    void paintBackground(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AxisEQAudioProcessorEditor)
};
