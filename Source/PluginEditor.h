#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/EQCurveComponent.h"
#include "GUI/ControlPanelComponent.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/CustomLookAndFeel.h"
#include "Utilities/PresetManager.h"

//==============================================================================
/// OmniQ top-level editor. Owns:
///   - TopBarComponent  (logo | preset browser | output gain | bypass)
///   - EQCurveComponent (spectrum + interactive curve + nodes)
///   - ControlPanelComponent (per-band strip, docked at the bottom)
class OmniQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit OmniQAudioProcessorEditor(OmniQAudioProcessor&);
    ~OmniQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    OmniQAudioProcessor& processorRef;

    // ── Preset system ────────────────────────────────────────────────────
    std::unique_ptr<OmniQ::PresetManager> presetManager;

    // ── Spectrum thread ──────────────────────────────────────────────────
    std::unique_ptr<OmniQ::SpectrumAnalyzer> analyzer;
    std::unique_ptr<OmniQ::EQCurveComponent> eqCurve;
    std::unique_ptr<OmniQ::ControlPanelComponent> controlPanel;

    OmniQ::CustomLookAndFeel customLookAndFeel;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OmniQAudioProcessorEditor)
};
