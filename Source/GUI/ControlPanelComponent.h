#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"

namespace AxisEQ
{

/// Always-visible bottom strip.
/// Shows nothing (hint text) when no band is selected.
/// Shows full per-band controls when a band node is clicked.
class ControlPanelComponent : public juce::Component
{
public:
    static constexpr int PanelHeight = 136;

    explicit ControlPanelComponent(AxisEQAudioProcessor& processor);
    ~ControlPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedBand(int bandIndex);
    int  getSelectedBand() const noexcept { return currentBandIndex; }

private:
    AxisEQAudioProcessor& processor;
    int currentBandIndex = -1;

    // ── Core parameter knobs ──────────────────────────────────────────────
    juce::Slider freqSlider, gainSlider, qSlider;
    juce::Label  freqLabel,  gainLabel,  qLabel;

    // ── Filter-type / slope / routing ─────────────────────────────────────
    juce::ComboBox typeCombo, slopeCombo, routingCombo;
    juce::Label    typeLabel, slopeLabel, routingLabel;

    // ── Dynamic EQ controls ───────────────────────────────────────────────
    juce::TextButton dynToggle { "DYNAMIC" };
    juce::TextButton dynScToggle { "EXT SC" };
    juce::Slider dynThreshSlider, dynRangeSlider, dynAttackSlider, dynReleaseSlider;
    juce::Label  dynThreshLabel,  dynRangeLabel,  dynAttackLabel,  dynReleaseLabel;

    // ── Active / Bypass ───────────────────────────────────────────────────
    juce::TextButton activeBtn { "Active" };
    juce::TextButton bypassBtn { "Bypass" };

    // ── APVTS Attachments ─────────────────────────────────────────────────
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    freqAttach, gainAttach, qAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>  typeAttach, slopeAttach, routingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>    dynToggleAttach, dynScAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    dynThreshAttach, dynRangeAttach,
                                                                              dynAttackAttach, dynReleaseAttach;

    // ── Helpers ───────────────────────────────────────────────────────────
    void setupKnob   (juce::Slider& s, juce::Label& l, const juce::String& caption);
    void setupDynKnob(juce::Slider& s, juce::Label& l, const juce::String& caption);
    void setupCombo  (juce::ComboBox& c, juce::Label& l,
                      const juce::StringArray& items, const juce::String& caption);
    void rebuildAttachments();
    void updateVisibility();

    // The editor draws the card behind the strip, so the strip itself is transparent.
    static juce::Colour bgCol()     { return juce::Colours::transparentBlack; }
    static juce::Colour accent()    { return Theme::accent; }
    static juce::Colour textHi()    { return Theme::text; }
    static juce::Colour textLo()    { return Theme::textDim; }
    static juce::Colour border()    { return Theme::track; }
    static juce::Colour dynAccent() { return juce::Colour(0xFF9B5FFF); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanelComponent)
};

} // namespace AxisEQ
