#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "RackUI.h"

namespace AxisEQ
{

/// Engage switch: an IN cap with a red LED, lit while the plugin is engaged. Attached to the
/// bypass parameter, so it lights while the toggle is off.
class PowerSwitch : public juce::ToggleButton
{
public:
    PowerSwitch() { setTooltip("Engage / bypass"); }

    // Red LED beside an IN cap; lit while the plugin is engaged (the bypass toggle is off).
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat();
        rackui::led(g, { 6.0f, r.getCentreY() }, 4.5f, rackui::colour::red, ! getToggleState());
        auto capArea = r.withTrimmedLeft(18.0f).reduced(0.0f, 2.0f);
        rackui::cap(g, capArea, over, down);
        if (down)
            capArea.translate(0.0f, 0.5f);
        rackui::print(g, "IN", capArea, rackui::Face::legend, 11.5f, 0.2f);
    }
};

} // namespace AxisEQ
