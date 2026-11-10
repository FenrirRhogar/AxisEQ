#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/EQCurveComponent.h"
#include "GUI/ControlPanelComponent.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/CustomLookAndFeel.h"
#include "GUI/PresetBar.h"
#include "GUI/PowerSwitch.h"
#include "Utilities/PresetManager.h"
#include "UpdateCheck.h"

//==============================================================================
/// AxisEQ top-level editor:
///   - header: Ansuz mark and wordmark | preset bar | power switch
///   - EQCurveComponent (spectrum + interactive curve + nodes) in a glass card
///   - ControlPanelComponent (per-band strip) in a card along the bottom
class AxisEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AxisEQAudioProcessorEditor(AxisEQAudioProcessor&);
    ~AxisEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int HeaderHeight = 74, Margin = 30, FooterHeight = 24, Ear = rackui::earWidth;

    AxisEQAudioProcessor& processorRef;
    AxisEQ::PresetManager* presetManager = nullptr;   // owned by the processor
    AxisEQ::CustomLookAndFeel customLookAndFeel;

    std::unique_ptr<AxisEQ::SpectrumAnalyzer> analyzer;
    std::unique_ptr<AxisEQ::EQCurveComponent> eqCurve;
    std::unique_ptr<AxisEQ::ControlPanelComponent> controlPanel;
    std::unique_ptr<AxisEQ::PresetBar> presetBar;
    AxisEQ::PowerSwitch power;
    std::unique_ptr<fenrikos::UpdateBadge> updateBadge;   // "AXISEQ 1.1.0" in the footer, or "UPDATE ... AVAILABLE"
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;

    juce::Rectangle<int> canvasCard, panelCard;
    int headerDividerX = 0;
    juce::Image background;   // everything static, redrawn only on resize

    void onSavePreset();
    void paintBackground(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AxisEQAudioProcessorEditor)
};
