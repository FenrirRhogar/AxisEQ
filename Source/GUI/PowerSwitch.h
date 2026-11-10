#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace AxisEQ
{

/// Illuminated push button, lit while the plugin is engaged. Attached to the
/// bypass parameter, so it lights while the toggle is off.
class PowerSwitch : public juce::ToggleButton
{
public:
    PowerSwitch() { setTooltip("Engage / bypass"); }

    // Illuminated IN button: lit while the plugin is engaged (the bypass toggle is off).
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        rack::illuminatedButton(g, getLocalBounds().toFloat(), Theme::accent, ! getToggleState(), over, down, "IN");
    }
};

} // namespace AxisEQ
