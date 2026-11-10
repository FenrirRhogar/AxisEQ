#include "CustomLookAndFeel.h"
#include "Theme.h"

namespace AxisEQ
{

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxTextColourId, Theme::text);
    setColour(juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha(0.3f));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF1B1E23));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha(0.3f));
    setColour(juce::PopupMenu::textColourId, Theme::text);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::PopupMenu::headerTextColourId, Theme::textDim);
    setColour(juce::Label::textColourId, Theme::text);
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xFF1D2025));
    setColour(juce::AlertWindow::textColourId, Theme::text);
    setColour(juce::TextEditor::backgroundColourId, Theme::glass);
    setColour(juce::TextEditor::textColourId, Theme::text);
    setColour(juce::TextEditor::outlineColourId, Theme::track);
    setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
}

//──────────────────────────────────────────────────────────────────────────────
// Knobs
//──────────────────────────────────────────────────────────────────────────────
void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, const float rotaryStartAngle,
                                         const float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float ringR = juce::jmax(12.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f);
    rack::knob(g, knobCache, bounds.getCentre(), ringR, rotaryStartAngle, rotaryEndAngle, sliderPos,
               slider.findColour(juce::Slider::rotarySliderFillColourId), slider.isEnabled(),
               slider.isMouseOverOrDragging());
}

juce::Label* CustomLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
    l->setJustificationType(juce::Justification::centred);
    l->setFont(Theme::font(11.0f));
    l->setColour(juce::Label::textColourId, Theme::text);
    l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

//──────────────────────────────────────────────────────────────────────────────
// Buttons: pills. Toggle buttons light up in their buttonOnColourId when on.
//──────────────────────────────────────────────────────────────────────────────
void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isMouseOverButton, bool isButtonDown)
{
    const bool on = button.getClickingTogglesState() && button.getToggleState();
    rack::pushButton(g, button.getLocalBounds().toFloat(), isMouseOverButton && button.isEnabled(), isButtonDown, on);
}

// Toggle buttons carry an LED in their on colour; the legend is silkscreened on the cap.
void CustomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool isMouseOverButton, bool isButtonDown)
{
    const bool toggle = button.getClickingTogglesState();
    const bool on = toggle && button.getToggleState();
    auto area = button.getLocalBounds().toFloat().reduced(1.0f, 1.5f);
    if (on || isButtonDown)
        area.translate(0.0f, 1.0f);
    const auto alpha = button.isEnabled() ? 1.0f : 0.4f;

    if (toggle)
    {
        const auto lit = button.findColour(juce::TextButton::buttonOnColourId).withAlpha(1.0f);
        rack::led(g, { area.getX() + 10.0f, area.getCentreY() }, juce::jmin(2.8f, area.getHeight() * 0.18f), lit, on && button.isEnabled());
        area.removeFromLeft(14.0f);
    }
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold).withKerningFactor(0.14f));
    g.setColour((isMouseOverButton || on ? Theme::text : Theme::text.withAlpha(0.8f)).withMultipliedAlpha(alpha));
    g.drawText(button.getButtonText().toUpperCase(), area, juce::Justification::centred, false);
}

//──────────────────────────────────────────────────────────────────────────────
// Combo boxes: small recessed display windows with a caret
//──────────────────────────────────────────────────────────────────────────────
void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto area = juce::Rectangle<int>(width, height).toFloat().reduced(1.5f);
    rack::recess(g, area, 2.5f, rack::colours::lcd);
    if (box.isMouseOver(true))
    {
        g.setColour(Theme::accent.withAlpha(0.06f));
        g.fillRoundedRectangle(area, 2.5f);
    }

    juce::Path caret;
    const float cx = area.getRight() - 10.0f, cy = area.getCentreY();
    caret.startNewSubPath(cx - 3.5f, cy - 1.5f);
    caret.lineTo(cx, cy + 2.0f);
    caret.lineTo(cx + 3.5f, cy - 1.5f);
    g.setColour(Theme::textDim);
    g.strokePath(caret, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::bold);
}

void CustomLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setColour(juce::Label::textColourId, Theme::accent.brighter(0.5f));
}

//──────────────────────────────────────────────────────────────────────────────
// Popup menus
//──────────────────────────────────────────────────────────────────────────────
juce::Font CustomLookAndFeel::getPopupMenuFont() { return Theme::font(13.5f); }

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(Theme::cardEdge);
    g.drawRect(0, 0, width, height);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          const bool isSeparator, const bool isActive,
                                          const bool isHighlighted, const bool isTicked,
                                          const bool /*hasSubMenu*/, const juce::String& text,
                                          const juce::String& /*shortcutKeyText*/,
                                          const juce::Drawable* /*icon*/, const juce::Colour* const /*textColourToUse*/)
{
    if (isSeparator)
    {
        auto r = area.reduced(8, 0);
        r.removeFromTop(juce::roundToInt((r.getHeight() * 0.5f) - 0.5f));
        g.setColour(Theme::cardEdge.withAlpha(0.15f));
        g.fillRect(r.removeFromTop(1));
        return;
    }

    auto textColour = findColour(isHighlighted ? juce::PopupMenu::highlightedTextColourId
                                               : juce::PopupMenu::textColourId);
    if (! isActive)
        textColour = textColour.withAlpha(0.5f);

    const auto r = area.reduced(2, 1);
    if (isHighlighted && isActive)
    {
        g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
    }

    auto textRect = r.reduced(14, 0);
    if (isTicked)
        Theme::led(g, { (float) textRect.getX() - 6.0f, (float) textRect.getCentreY() }, 2.5f, Theme::accent, true);

    g.setColour(textColour);
    g.setFont(getPopupMenuFont());
    g.drawText(text, textRect, juce::Justification::centredLeft, true);
}

} // namespace AxisEQ
