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
    const auto centre = bounds.getCentre();
    const float ringR = juce::jmax(6.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f);
    const float r = ringR - 5.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto colour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const bool active = slider.isMouseOverOrDragging() && slider.isEnabled();

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringR, ringR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Theme::track);
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Gain-style controls (range crossing zero) light from the centre, the rest from the start.
    const auto range = slider.getRange();
    const float origin = range.getStart() < 0.0 && range.getEnd() > 0.0
                             ? (float) slider.valueToProportionOfLength(0.0) : 0.0f;
    const float originAngle = rotaryStartAngle + origin * (rotaryEndAngle - rotaryStartAngle);
    if (slider.isEnabled() && std::abs(angle - originAngle) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, ringR, ringR, 0.0f,
                            juce::jmin(originAngle, angle), juce::jmax(originAngle, angle), true);
        Theme::glow(g, value, active ? colour.brighter(0.25f) : colour, 3.0f);
    }

    // Body: soft shadow, convex rim and a slightly dished face.
    const auto body = juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(body.translated(0.0f, 2.5f).expanded(1.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF40444D), body.getCentreX(), body.getY(),
                                           juce::Colour(0xFF1A1C21), body.getCentreX(), body.getBottom(), false));
    g.fillEllipse(body);
    const auto face = body.reduced(r * 0.14f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF23262C), face.getCentreX(), face.getY(),
                                           juce::Colour(0xFF30343B), face.getCentreX(), face.getBottom(), false));
    g.fillEllipse(face);
    g.setColour(juce::Colours::white.withAlpha(active ? 0.12f : 0.06f));
    g.drawEllipse(body.reduced(0.5f), 1.0f);

    const auto polar = [&](float radius) {
        return centre.translated(radius * std::sin(angle), -radius * std::cos(angle));
    };
    juce::Path indicator;
    indicator.startNewSubPath(polar(r * 0.3f));
    indicator.lineTo(polar(r * 0.8f));
    Theme::glow(g, indicator, slider.isEnabled() ? colour.brighter(0.3f) : Theme::textDim, 2.5f);
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
    const auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    const float corner = area.getHeight() * 0.5f;
    const bool on = button.getClickingTogglesState() && button.getToggleState();
    const auto lit = button.findColour(juce::TextButton::buttonOnColourId);

    if (on)
    {
        g.setColour(lit.withAlpha(0.22f));
        g.fillRoundedRectangle(area, corner);
        g.setColour(lit.withAlpha(isMouseOverButton ? 1.0f : 0.8f));
        g.drawRoundedRectangle(area, corner, 1.0f);
        return;
    }

    g.setColour(Theme::button.brighter(isButtonDown ? 0.02f : (isMouseOverButton ? 0.12f : 0.0f)));
    g.fillRoundedRectangle(area, corner);
    g.setColour(isMouseOverButton ? Theme::accent.withAlpha(0.45f) : Theme::cardEdge);
    g.drawRoundedRectangle(area, corner, 1.0f);
}

void CustomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool isMouseOverButton, bool)
{
    const bool on = button.getClickingTogglesState() && button.getToggleState();
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold).withKerningFactor(0.08f));
    g.setColour(on ? button.findColour(juce::TextButton::buttonOnColourId).brighter(0.4f)
                   : (isMouseOverButton ? Theme::text : Theme::text.withAlpha(0.85f)));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

//──────────────────────────────────────────────────────────────────────────────
// Combo boxes: pills with a caret
//──────────────────────────────────────────────────────────────────────────────
void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto area = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);
    const float corner = juce::jmin(8.0f, area.getHeight() * 0.5f);
    const bool over = box.isMouseOver(true);
    g.setColour(Theme::glass.brighter(over ? 0.08f : 0.0f));
    g.fillRoundedRectangle(area, corner);
    g.setColour(over ? Theme::accent.withAlpha(0.5f) : Theme::cardEdge.withAlpha(0.12f));
    g.drawRoundedRectangle(area, corner, 1.0f);

    juce::Path caret;
    const float cx = area.getRight() - 12.0f, cy = area.getCentreY();
    caret.startNewSubPath(cx - 3.5f, cy - 1.5f);
    caret.lineTo(cx, cy + 2.0f);
    caret.lineTo(cx + 3.5f, cy - 1.5f);
    g.setColour(Theme::textDim);
    g.strokePath(caret, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox&) { return Theme::font(12.0f); }

void CustomLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setColour(juce::Label::textColourId, Theme::text);
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
