#pragma once

#include <JuceHeader.h>
#include "RackKit.h"

namespace AxisEQ
{

/// Visual language shared with the other fenrikos plugins: a black anodised rack plate
/// with silkscreen legends, machined knobs and the rune mark.
namespace Theme
{
    const juce::Colour plateTop   = rack::colours::plateTop;
    const juce::Colour plateBottom= rack::colours::plateBottom;
    const juce::Colour cardFill   { 0xFF222428 };
    const juce::Colour cardEdge   { 0x14FFFFFF };
    const juce::Colour glass      = rack::colours::glass;
    const juce::Colour text       = rack::colours::silk;
    const juce::Colour textDim    = rack::colours::silkDim;
    const juce::Colour track      { 0xFF3A3C41 };
    const juce::Colour button     { 0xFF2E3035 };
    const juce::Colour accent     { 0xFF4C8DFF };   // AxisEQ blue

    inline juce::Font font(float height, bool bold = false)
    {
        return juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain);
    }

    inline juce::Font mono(float height)
    {
        return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain);
    }

    /// Stroke with a soft bloom, for lit arcs and traces.
    inline void glow(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        g.setColour(colour.withAlpha(0.16f));
        g.strokePath(path, juce::PathStrokeType(width * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void led(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
    {
        rack::led(g, centre, radius, colour, on);
    }

    /// On the rack plate a panel is a silkscreened outline.
    inline void card(juce::Graphics& g, juce::Rectangle<float> area, float /*corner*/ = 12.0f, const juce::String& title = {})
    {
        rack::silkBox(g, area, title);
    }

    /// The AxisEQ mark: Ansuz, the A rune. An upright stave with two arms slanting
    /// down to the right, as round-capped strokes inside a square box.
    inline juce::Path ansuz(juce::Rectangle<float> box)
    {
        const float size = juce::jmin(box.getWidth(), box.getHeight());
        const auto at = [&](float x, float y) { return juce::Point<float>(box.getX() + x * size, box.getY() + y * size); };

        juce::Path lines;
        for (auto [from, to] : { std::pair { at(0.36f, 0.10f), at(0.36f, 0.90f) },
                                 { at(0.36f, 0.12f), at(0.74f, 0.40f) },
                                 { at(0.36f, 0.38f), at(0.74f, 0.66f) } })
        {
            lines.startNewSubPath(from);
            lines.lineTo(to);
        }

        juce::Path rune;
        juce::PathStrokeType(0.13f * size, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
            .createStrokedPath(rune, lines);
        return rune;
    }

    /// The fenrikos brand gradient for rune marks, amber to red.
    inline void fillRune(juce::Graphics& g, const juce::Path& rune)
    {
        const auto b = rune.getBounds();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFFC45A), b.getCentreX(), b.getY(),
                                               juce::Colour(0xFFE84830), b.getCentreX(), b.getBottom(), false));
        g.fillPath(rune);
    }
}

} // namespace AxisEQ
