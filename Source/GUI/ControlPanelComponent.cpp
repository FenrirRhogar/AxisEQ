#include "ControlPanelComponent.h"
#include "../Utilities/Constants.h"

namespace OmniQ
{

ControlPanelComponent::ControlPanelComponent(OmniQAudioProcessor& p) : processor(p)
{
    setupKnob(freqSlider, freqLabel, "Frequency");
    freqSlider.setSkewFactorFromMidPoint(1000.f);

    setupKnob(gainSlider, gainLabel, "Gain");

    setupKnob(qSlider, qLabel, "Q");
    qSlider.setSkewFactorFromMidPoint(1.f);

    setupCombo(typeCombo,    typeLabel,    FilterTypeNames,  "Type");
    setupCombo(slopeCombo,   slopeLabel,   FilterSlopeNames, "Slope");
    setupCombo(routingCombo, routingLabel, RoutingModeNames, "Route");

    // We will style dynToggle and dynScToggle below along with other TextButtons

    setupDynKnob(dynThreshSlider,  dynThreshLabel,  "Thresh");
    setupDynKnob(dynRangeSlider,   dynRangeLabel,   "Range");
    setupDynKnob(dynAttackSlider,  dynAttackLabel,  "Attack");
    setupDynKnob(dynReleaseSlider, dynReleaseLabel, "Release");

    // Active / Bypass toggle buttons
    auto styleBtn = [&](juce::TextButton& btn, juce::Colour onCol)
    {
        btn.setClickingTogglesState(true);
        btn.setColour(juce::TextButton::buttonColourId,   border());
        btn.setColour(juce::TextButton::buttonOnColourId, onCol);
        btn.setColour(juce::TextButton::textColourOffId,  textLo());
        btn.setColour(juce::TextButton::textColourOnId,   textHi());
        addAndMakeVisible(btn);
    };
    styleBtn(activeBtn, accent());
    styleBtn(bypassBtn, juce::Colour(0xFFE8B44A));
    styleBtn(dynToggle, dynAccent());
    styleBtn(dynScToggle, dynAccent().withAlpha(0.6f));

    activeBtn.onClick = [this]
    {
        if (currentBandIndex < 0) return;
        if (auto* param = processor.apvts.getParameter(paramID("active", currentBandIndex)))
            param->setValueNotifyingHost(activeBtn.getToggleState() ? 1.f : 0.f);
    };
    bypassBtn.onClick = [this]
    {
        if (currentBandIndex < 0) return;
        if (auto* param = processor.apvts.getParameter(paramID("bypass", currentBandIndex)))
            param->setValueNotifyingHost(bypassBtn.getToggleState() ? 1.f : 0.f);
    };

    dynToggle.onClick = [this] { updateVisibility(); };
    typeCombo.onChange = [this] { updateVisibility(); };

    updateVisibility();
}

ControlPanelComponent::~ControlPanelComponent() = default;

//──────────────────────────────────────────────────────────────────────────────
void ControlPanelComponent::setSelectedBand(int idx)
{
    if (currentBandIndex == idx) return;
    currentBandIndex = idx;
    rebuildAttachments();
    updateVisibility();
    resized();
    repaint();
}

//──────────────────────────────────────────────────────────────────────────────
// Setup helpers
//──────────────────────────────────────────────────────────────────────────────
void ControlPanelComponent::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& cap)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 15);
    s.setColour(juce::Slider::rotarySliderFillColourId,    accent());
    s.setColour(juce::Slider::rotarySliderOutlineColourId, border());
    s.setColour(juce::Slider::thumbColourId,               accent().brighter(0.3f));
    s.setColour(juce::Slider::textBoxTextColourId,         textHi());
    s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentWhite);
    s.setColour(juce::Slider::textBoxBackgroundColourId,   border().withAlpha(0.6f));
    addAndMakeVisible(s);

    l.setText(cap, juce::dontSendNotification);
    l.setFont(juce::FontOptions(9.5f));
    l.setColour(juce::Label::textColourId, textLo());
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void ControlPanelComponent::setupDynKnob(juce::Slider& s, juce::Label& l, const juce::String& cap)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId,    dynAccent());
    s.setColour(juce::Slider::rotarySliderOutlineColourId, border());
    s.setColour(juce::Slider::thumbColourId,               dynAccent().brighter(0.3f));
    s.setColour(juce::Slider::textBoxTextColourId,         textHi());
    s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentWhite);
    s.setColour(juce::Slider::textBoxBackgroundColourId,   border().withAlpha(0.6f));
    addAndMakeVisible(s);

    l.setText(cap, juce::dontSendNotification);
    l.setFont(juce::FontOptions(9.5f));
    l.setColour(juce::Label::textColourId, textLo());
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void ControlPanelComponent::setupCombo(juce::ComboBox& c, juce::Label& l,
                                        const juce::StringArray& items, const juce::String& cap)
{
    c.addItemList(items, 1);
    c.setJustificationType(juce::Justification::centred);
    c.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    c.setColour(juce::ComboBox::textColourId,       textHi().withAlpha(0.85f));
    c.setColour(juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
    c.setColour(juce::ComboBox::arrowColourId,      textLo().withAlpha(0.6f));
    addAndMakeVisible(c);

    l.setText(cap, juce::dontSendNotification);
    l.setFont(juce::FontOptions(9.5f));
    l.setColour(juce::Label::textColourId, textLo().withAlpha(0.5f));
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

//──────────────────────────────────────────────────────────────────────────────
void ControlPanelComponent::rebuildAttachments()
{
    freqAttach.reset(); gainAttach.reset(); qAttach.reset();
    typeAttach.reset(); slopeAttach.reset(); routingAttach.reset();
    dynToggleAttach.reset(); dynScAttach.reset();
    dynThreshAttach.reset(); dynRangeAttach.reset();
    dynAttackAttach.reset(); dynReleaseAttach.reset();

    if (currentBandIndex < 0) return;
    const int b = currentBandIndex;
    auto& apvts = processor.apvts;

    freqAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (apvts, paramID("freq",          b), freqSlider);
    gainAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (apvts, paramID("gain",          b), gainSlider);
    qAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (apvts, paramID("q",             b), qSlider);
    typeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramID("type",          b), typeCombo);
    slopeAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramID("slope",         b), slopeCombo);
    routingAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramID("route",         b), routingCombo);
    dynToggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID("dyn_enabled",   b), dynToggle);
    dynScAttach     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID("dyn_ext_sc",    b), dynScToggle);
    dynThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID("dyn_threshold", b), dynThreshSlider);
    dynRangeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID("dyn_range",     b), dynRangeSlider);
    dynAttackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID("dyn_attack",    b), dynAttackSlider);
    dynReleaseAttach= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID("dyn_release",   b), dynReleaseSlider);

    // Sync toggle buttons
    activeBtn.setToggleState(processor.bandParams[static_cast<size_t>(b)].isActive(),   juce::dontSendNotification);
    bypassBtn.setToggleState(processor.bandParams[static_cast<size_t>(b)].isBypassed(), juce::dontSendNotification);
}

void ControlPanelComponent::updateVisibility()
{
    const bool has = (currentBandIndex >= 0);

    std::vector<juce::Component*> all {
        &freqSlider,   &gainSlider,   &qSlider,
        &freqLabel,    &gainLabel,    &qLabel,
        &typeCombo,    &typeLabel,
        &slopeCombo,   &slopeLabel,
        &routingCombo, &routingLabel,
        &dynToggle,    &dynScToggle,
        &dynThreshSlider, &dynRangeSlider,
        &dynAttackSlider, &dynReleaseSlider,
        &dynThreshLabel,  &dynRangeLabel,
        &dynAttackLabel,  &dynReleaseLabel,
        &activeBtn, &bypassBtn
    };

    for (auto* c : all)
        c->setVisible(has);

    if (has)
    {
        const bool dynOn = dynToggle.getToggleState();
        dynScToggle.setEnabled(dynOn);
        dynThreshSlider.setEnabled(dynOn);
        dynRangeSlider.setEnabled(dynOn);
        dynAttackSlider.setEnabled(dynOn);
        dynReleaseSlider.setEnabled(dynOn);
        
        bool oldSlopeVis = slopeCombo.isVisible();
        const bool usesSlope = processor.bandParams[static_cast<size_t>(currentBandIndex)].filterUsesSlope();
        slopeCombo.setVisible(usesSlope);
        slopeLabel.setVisible(usesSlope);
        
        if (oldSlopeVis != usesSlope)
            resized();
    }
}

//──────────────────────────────────────────────────────────────────────────────
// paint
//──────────────────────────────────────────────────────────────────────────────
void ControlPanelComponent::paint(juce::Graphics& g)
{
    g.setColour(bgCol());
    g.fillRect(getLocalBounds().toFloat());

    if (currentBandIndex < 0)
    {
        g.setColour(textLo());
        g.setFont(juce::FontOptions(12.5f));
        g.drawText("Click anywhere on the EQ canvas to add a band  ·  Right-click a node for options",
                   getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    // Band colour stripe on the left edge
    g.setColour(BandColours[static_cast<size_t>(currentBandIndex)]);
    g.fillRect(juce::Rectangle<float>(0.f, 0.f, 3.f, static_cast<float>(getHeight())));

    // Column dividers
    const float w   = static_cast<float>(getWidth());
    const float h   = static_cast<float>(getHeight());
    const float d1  = w * 0.30f;
    const float d2  = w * 0.56f;
    g.setColour(border().withAlpha(0.8f));
    g.drawVerticalLine(static_cast<int>(d1), 4.f, h - 4.f);
    g.drawVerticalLine(static_cast<int>(d2), 4.f, h - 4.f);

    // Section headers
    g.setFont(juce::FontOptions(9.f));
    g.setColour(textLo());
    g.drawText("FILTER", juce::Rectangle<float>(4.f,  0.f, d1 - 4.f, 14.f), juce::Justification::centred);
    g.drawText("EQ",     juce::Rectangle<float>(d1,   0.f, d2 - d1,  14.f), juce::Justification::centred);
    g.drawText("DYNAMICS",juce::Rectangle<float>(d2,  0.f, w - d2,   14.f), juce::Justification::centred);

    // Band title
    g.setColour(BandColours[static_cast<size_t>(currentBandIndex)]);
    g.setFont(juce::FontOptions(10.f).withStyle("Bold"));
    g.drawText("Band " + juce::String(currentBandIndex + 1),
               4, 0, 60, 14, juce::Justification::centredLeft, false);
}

//──────────────────────────────────────────────────────────────────────────────
// resized
//──────────────────────────────────────────────────────────────────────────────
void ControlPanelComponent::resized()
{
    if (currentBandIndex < 0) return;

    const int w   = getWidth();
    const int h   = getHeight();
    const int top = 14;          // space for section headers
    const int bot = h - top;

    const int col1End = static_cast<int>(w * 0.30f);
    const int col2End = static_cast<int>(w * 0.56f);

    // ── Column 1: Type / Slope / Routing / Active / Bypass ────────────────
    {
        const int cw    = col1End - 8;
        const int cx    = 5;
        const int lh    = 11;
        const int combH = 20;
        int y = top + 2;

        typeLabel.setBounds   (cx, y,       cw, lh);         y += lh;
        typeCombo.setBounds   (cx, y,       cw, combH);       y += combH + 3;

        if (slopeCombo.isVisible())
        {
            slopeLabel.setBounds  (cx, y,       cw, lh);         y += lh;
            slopeCombo.setBounds  (cx, y,       cw, combH);       y += combH + 3;
        }

        routingLabel.setBounds(cx, y,       cw, lh);         y += lh;
        routingCombo.setBounds(cx, y,       cw, combH);       y += combH + 4;

        const int btnW = (cw - 3) / 2;
        activeBtn.setBounds(cx,            y, btnW, 18);
        bypassBtn.setBounds(cx + btnW + 3, y, btnW, 18);
    }

    // ── Column 2: Freq / Gain / Q knobs ──────────────────────────────────
    {
        const int colW  = col2End - col1End;
        const int kw    = colW / 3;
        const int lh    = 11;
        const int kh    = bot - lh;
        int x = col1End;

        auto place = [&](juce::Slider& s, juce::Label& l)
        {
            l.setBounds(x, top, kw, lh);
            s.setBounds(x, top + lh, kw, kh);
            x += kw;
        };
        place(freqSlider, freqLabel);
        place(gainSlider, gainLabel);
        place(qSlider,    qLabel);
    }

    // ── Column 3: Dynamics ────────────────────────────────────────────────
    {
        const int col3W = w - col2End;
        const int col3X = col2End;
        const int lh    = 11;
        const int togH  = 16;
        const int dynH  = bot - togH - lh - 2;
        const int kw    = (col3W - 4) / 4;

        int halfTog = (col3W - 4) / 2;
        dynToggle.setBounds(col3X + 2, top + 1, halfTog, togH);
        dynScToggle.setBounds(col3X + 2 + halfTog, top + 1, halfTog, togH);

        int x = col3X + 2;
        int y = top + lh + togH + 2;

        auto place = [&](juce::Slider& s, juce::Label& l)
        {
            l.setBounds(x, y,           kw, lh);
            s.setBounds(x, y + lh,      kw, dynH - lh);
            x += kw;
        };
        place(dynThreshSlider,  dynThreshLabel);
        place(dynRangeSlider,   dynRangeLabel);
        place(dynAttackSlider,  dynAttackLabel);
        place(dynReleaseSlider, dynReleaseLabel);
    }
}

} // namespace OmniQ
