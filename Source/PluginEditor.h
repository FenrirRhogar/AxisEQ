#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/EQCurveComponent.h"
#include "GUI/ControlPanelComponent.h"
#include "GUI/SpectrumAnalyzer.h"
#include "Utilities/PresetManager.h"

//==============================================================================
/// ProEQ top-level editor. Owns:
///   - TopBarComponent  (logo | preset browser | output gain | bypass)
///   - EQCurveComponent (spectrum + interactive curve + nodes)
///   - ControlPanelComponent (per-band strip, docked at the bottom)
class ProEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ProEQAudioProcessorEditor(ProEQAudioProcessor&);
    ~ProEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ProEQAudioProcessor& processorRef;

    // ── Preset system ────────────────────────────────────────────────────
    std::unique_ptr<ProEQ::PresetManager> presetManager;

    // ── Spectrum thread ──────────────────────────────────────────────────
    std::unique_ptr<ProEQ::SpectrumAnalyzer> analyzer;

    // ── Main display ─────────────────────────────────────────────────────
    std::unique_ptr<ProEQ::EQCurveComponent> eqCurve;

    // ── Per-band bottom strip ─────────────────────────────────────────────
    std::unique_ptr<ProEQ::ControlPanelComponent> controlPanel;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProEQAudioProcessorEditor)
};
