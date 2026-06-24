#pragma once
#include <JuceHeader.h>

namespace OmniQ
{

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;

    // Rotary sliders
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override;

    // Text editors (value boxes)
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    
    // Popup menus
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           const bool isSeparator, const bool isActive,
                           const bool isHighlighted, const bool isTicked,
                           const bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* const textColourToUse) override;

    // Fonts
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;

private:
    juce::Typeface::Ptr customTypeface;
};

} // namespace OmniQ
