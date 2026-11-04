#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace AxisEQ
{

/// Pill switch with a glowing track while the plugin is engaged. Attached to the
/// bypass parameter, so it lights while the toggle is off.
class PowerSwitch : public juce::ToggleButton
{
public:
    PowerSwitch() { setTooltip("Engage / bypass"); }

    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        const auto area = getLocalBounds().toFloat().reduced(2.0f);
        const auto track = area.withSizeKeepingCentre(area.getWidth(), juce::jmin(area.getHeight(), area.getWidth() * 0.55f));
        const float radius = track.getHeight() * 0.5f;
        const bool engaged = ! getToggleState();

        if (engaged)
        {
            g.setColour(Theme::accent.withAlpha(0.2f));
            g.fillRoundedRectangle(track.expanded(2.5f), radius + 2.5f);
        }
        g.setColour(engaged ? Theme::accent.withAlpha(0.9f) : Theme::button);
        g.fillRoundedRectangle(track, radius);
        g.setColour(over ? juce::Colours::white.withAlpha(0.2f) : Theme::cardEdge);
        g.drawRoundedRectangle(track, radius, 1.0f);

        const float thumb = track.getHeight() - 5.0f;
        const float thumbX = engaged ? track.getRight() - thumb - 2.5f : track.getX() + 2.5f;
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillEllipse(thumbX, track.getY() + 3.5f, thumb, thumb);
        g.setColour(engaged ? juce::Colours::white : juce::Colour(0xFF8A909B));
        g.fillEllipse(thumbX, track.getY() + 2.5f, thumb, thumb);
    }
};

} // namespace AxisEQ
