#include "CustomLookAndFeel.h"

namespace OmniQ
{

CustomLookAndFeel::CustomLookAndFeel()
{
    // Configure default colors for text boxes and popups
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xFF6320EE).withAlpha(0.3f));
    
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF14142B));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF6320EE).withAlpha(0.4f));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xFFDCDCDC));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Typeface::Ptr CustomLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    // Use system modern font (Segoe UI on Windows, San Francisco on Mac)
    static auto typeface = juce::Typeface::createSystemTypefaceFor(juce::Font("Segoe UI", f.getHeight(), f.getStyleFlags()));
    return typeface;
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, const float rotaryStartAngle,
                                         const float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto topx = bounds.getCentreX() - radius;
    auto topy = bounds.getCentreY() - radius;
    auto rw = radius * 2.0f;
    
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    
    // Background arc track
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), radius, radius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId).withAlpha(0.3f));
    g.strokePath(backgroundArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    
    // Filled value arc
    if (slider.isEnabled())
    {
        juce::Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), radius, radius,
                               0.0f, rotaryStartAngle, angle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(valueArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Thumb thumb
    juce::Point<float> thumbPoint(bounds.getCentreX() + radius * std::cos(angle - juce::MathConstants<float>::halfPi),
                                  bounds.getCentreY() + radius * std::sin(angle - juce::MathConstants<float>::halfPi));
    
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre(thumbPoint));
}

juce::Label* CustomLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
    l->setJustificationType(juce::Justification::centred);
    l->setFont(10.0f);
    return l;
}

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.setColour(findColour(juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 6.0f);
    
    g.setColour(findColour(juce::PopupMenu::backgroundColourId).brighter(0.2f));
    g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(width - 1), static_cast<float>(height - 1), 6.0f, 1.0f);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          const bool isSeparator, const bool isActive,
                                          const bool isHighlighted, const bool isTicked,
                                          const bool hasSubMenu, const juce::String& text,
                                          const juce::String& shortcutKeyText,
                                          const juce::Drawable* icon, const juce::Colour* const textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        r.removeFromTop(juce::roundToInt((r.getHeight() * 0.5f) - 0.5f));
        g.setColour(juce::Colour(0x33FFFFFF));
        g.fillRect(r.removeFromTop(1));
    }
    else
    {
        auto textColour = findColour(isHighlighted ? juce::PopupMenu::highlightedTextColourId
                                                   : juce::PopupMenu::textColourId);
        if (!isActive) textColour = textColour.withAlpha(0.5f);

        auto r = area.reduced(1);
        if (isHighlighted && isActive)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
        }

        g.setColour(textColour);
        g.setFont(12.0f);

        auto textRect = r.reduced(10, 0);
        
        if (isTicked)
        {
            // Draw a subtle dot or tick
            g.fillEllipse(textRect.getX() - 6.0f, textRect.getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        g.drawText(text, textRect, juce::Justification::centredLeft, true);
    }
}

} // namespace OmniQ
