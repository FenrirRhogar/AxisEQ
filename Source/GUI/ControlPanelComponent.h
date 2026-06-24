#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace OmniQ
{

/// Always-visible bottom strip.
/// Shows nothing (hint text) when no band is selected.
/// Shows full per-band controls when a band node is clicked.
class ControlPanelComponent : public juce::Component
{
public:
    static constexpr int PanelHeight = 120;

    explicit ControlPanelComponent(OmniQAudioProcessor& processor);
    ~ControlPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedBand(int bandIndex);
    int  getSelectedBand() const noexcept { return currentBandIndex; }

private:
    OmniQAudioProcessor& processor;
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

    static juce::Colour bgCol()     { return juce::Colour(0xFF0C0C1E); }
    static juce::Colour accent()    { return juce::Colour(0xFF3D84FF); }
    static juce::Colour textHi()    { return juce::Colour(0xFFE8E8FF); }
    static juce::Colour textLo()    { return juce::Colour(0xFF5A5A80); }
    static juce::Colour border()    { return juce::Colour(0xFF1C1C38); }
    static juce::Colour dynAccent() { return juce::Colour(0xFF9B5FFF); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanelComponent)
};

} // namespace OmniQ
