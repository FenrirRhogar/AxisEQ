#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "RackUI.h"
#include "Theme.h"

namespace AxisEQ
{

/// The band strip under the screen, laid out like hardware: the band's filter section (type, slope
/// and routing as LED selectors, ACTIVE and BYPASS), its frequency / gain / Q knobs and its
/// dynamics section. Shows a hint while no band is selected.
class ControlPanelComponent : public juce::Component, private juce::Timer
{
public:
    static constexpr int PanelHeight = 184;

    explicit ControlPanelComponent(AxisEQAudioProcessor& processor);
    ~ControlPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedBand(int bandIndex);
    int  getSelectedBand() const noexcept { return currentBandIndex; }

private:
    using Knob = rackui::Knob;

    void rebuildAttachments();
    void updateVisibility();
    void timerCallback() override;
    juce::Colour bandColour() const;

    AxisEQAudioProcessor& processor;
    int currentBandIndex = -1;

    Knob freqKnob, gainKnob, qKnob;
    Knob threshKnob, rangeKnob, attackKnob, releaseKnob;

    rackui::LedSelector typeSelect, slopeSelect, routeSelect;
    rackui::LedToggle activeToggle, bypassToggle, dynToggle, scToggle;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    std::array<int, 3> dividerX {};
    bool lastSlope = true, lastDyn = true;

    static juce::Colour dynAccent() { return juce::Colour(0xFF9B5FFF); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanelComponent)
};

} // namespace AxisEQ
