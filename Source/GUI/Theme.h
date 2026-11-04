#pragma once

#include <JuceHeader.h>

namespace AxisEQ
{

/// Visual language shared with the other fenrikos plugins: dark graphite cards,
/// hairline edges, glowing accent arcs and the rune mark.
namespace Theme
{
    const juce::Colour plateTop   { 0xFF1E2127 };
    const juce::Colour plateBottom{ 0xFF131519 };
    const juce::Colour cardFill   { 0xFF1A1D22 };
    const juce::Colour cardEdge   { 0x10FFFFFF };
    const juce::Colour glass      { 0xFF0C0E11 };
    const juce::Colour text       { 0xFFE2E5EA };
    const juce::Colour textDim    { 0xFF868C97 };
    const juce::Colour track      { 0xFF2F333B };
    const juce::Colour button     { 0xFF252930 };
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

    /// A small lit dot with a halo.
    inline void led(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
    {
        if (on)
        {
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.45f), centre, colour.withAlpha(0.0f),
                                                   centre.translated(radius * 3.5f, 0.0f), true));
            g.fillEllipse(juce::Rectangle<float>(radius * 7.0f, radius * 7.0f).withCentre(centre));
        }
        g.setColour(on ? colour.brighter(0.4f) : colour.darker(2.5f).withAlpha(0.8f));
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));
    }

    /// Raised card: soft shadow, slightly lighter top edge and a hairline border.
    inline void card(juce::Graphics& g, juce::Rectangle<float> area, float corner = 12.0f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(area.translated(0.0f, 3.0f).expanded(1.0f), corner + 1.0f);
        g.setGradientFill(juce::ColourGradient(cardFill.brighter(0.07f), area.getTopLeft(),
                                               cardFill, area.getBottomLeft(), false));
        g.fillRoundedRectangle(area, corner);
        g.setColour(cardEdge);
        g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
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
