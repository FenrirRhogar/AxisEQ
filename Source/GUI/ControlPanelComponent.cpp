#include "ControlPanelComponent.h"
#include "../Utilities/Constants.h"

namespace AxisEQ
{

namespace
{
    std::vector<rackui::Mark> marks(std::initializer_list<std::pair<double, const char*>> m)
    {
        std::vector<rackui::Mark> out;
        for (auto [v, t] : m)
            out.push_back({ v, t });
        return out;
    }

    const juce::Colour filterLed { 0xFFFF9A4A };
}

ControlPanelComponent::ControlPanelComponent(AxisEQAudioProcessor& p)
    : processor(p),
      freqKnob (64.0f, Knob::Variant::metal, Theme::accent, "FREQUENCY", "Hz",
                marks({ { 20, "20" }, { 100, "100" }, { 1000, "1k" }, { 5000, "5k" }, { 20000, "20k" } })),
      gainKnob (64.0f, Knob::Variant::metal, Theme::accent, "GAIN", "dB",
                marks({ { -30, "-30" }, { -15, "-15" }, { 0, "0" }, { 15, "+15" }, { 30, "+30" } })),
      qKnob    (48.0f, Knob::Variant::black, Theme::accent, "Q", "",
                marks({ { 0.1, "0.1" }, { 0.7, ".7" }, { 2, "2" }, { 10, "10" }, { 40, "40" } })),
      threshKnob (36.0f, Knob::Variant::black, dynAccent(), "THRESH", "dB", marks({ { -60, "-60" }, { 0, "0" } })),
      rangeKnob  (36.0f, Knob::Variant::black, dynAccent(), "RANGE", "dB", marks({ { -30, "-30" }, { 30, "+30" } })),
      attackKnob (36.0f, Knob::Variant::black, dynAccent(), "ATTACK", "ms", marks({ { 0.1, "0.1" }, { 500, "500" } })),
      releaseKnob(36.0f, Knob::Variant::black, dynAccent(), "RELEASE", "ms", marks({ { 1, "1" }, { 5000, "5k" } })),
      typeSelect ({ "BELL", "LOW CUT", "HIGH CUT", "LO SHELF", "HI SHELF", "NOTCH", "BAND PASS", "TILT" }, filterLed, true, 2),
      slopeSelect({ "6", "12", "24", "48", "96" }, filterLed),
      routeSelect({ "STEREO", "MID", "SIDE" }, filterLed),
      activeToggle("ACTIVE", filterLed),
      bypassToggle("BYPASS", juce::Colour(0xFFFFC928)),
      dynToggle("ON", dynAccent()),
      scToggle("EXT SC", dynAccent())
{
    for (auto* c : std::initializer_list<juce::Component*> { &freqKnob, &gainKnob, &qKnob, &threshKnob, &rangeKnob, &attackKnob,
                                                             &releaseKnob, &typeSelect, &slopeSelect, &routeSelect,
                                                             &activeToggle, &bypassToggle, &dynToggle, &scToggle })
        addChildComponent(c);
    startTimerHz(15);
}

ControlPanelComponent::~ControlPanelComponent() { stopTimer(); }

juce::Colour ControlPanelComponent::bandColour() const
{
    return currentBandIndex >= 0 ? BandColours[static_cast<size_t>(currentBandIndex)] : Theme::accent;
}

void ControlPanelComponent::setSelectedBand(int bandIndex)
{
    currentBandIndex = bandIndex;
    freqKnob.setAccent(bandColour());
    gainKnob.setAccent(bandColour());
    rebuildAttachments();
    updateVisibility();
    resized();
    repaint();
}

void ControlPanelComponent::rebuildAttachments()
{
    sliderAttachments.clear();
    buttonAttachments.clear();
    if (currentBandIndex < 0)
        return;

    auto& apvts = processor.apvts;
    const int b = currentBandIndex;
    const auto slider = [&](Knob& k, const char* name)
    {
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID(name, b), k));
        if (auto* param = apvts.getParameter(paramID(name, b)))
            k.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
        k.repaint();
    };
    slider(freqKnob, "freq");
    slider(gainKnob, "gain");
    slider(qKnob, "q");
    slider(threshKnob, "dyn_threshold");
    slider(rangeKnob, "dyn_range");
    slider(attackKnob, "dyn_attack");
    slider(releaseKnob, "dyn_release");

    const auto button = [&](juce::Button& t, const char* name)
    {
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID(name, b), t));
    };
    button(activeToggle, "active");
    button(bypassToggle, "bypass");
    button(dynToggle, "dyn_enabled");
    button(scToggle, "dyn_ext_sc");

    typeSelect.attach(*apvts.getParameter(paramID("type", b)));
    slopeSelect.attach(*apvts.getParameter(paramID("slope", b)));
    routeSelect.attach(*apvts.getParameter(paramID("route", b)));
}

void ControlPanelComponent::updateVisibility()
{
    const bool has = currentBandIndex >= 0;
    for (auto* c : getChildren())
        c->setVisible(has);
    timerCallback();
}

// Slope only applies to the cut filters; the dynamics knobs only while dynamics are on.
void ControlPanelComponent::timerCallback()
{
    if (currentBandIndex < 0)
        return;
    const auto& band = processor.bandParams[static_cast<size_t>(currentBandIndex)];
    const bool slope = band.filterUsesSlope();
    const bool dyn = dynToggle.getToggleState();
    if (slope != lastSlope)
    {
        lastSlope = slope;
        slopeSelect.setEnabled(slope);
        slopeSelect.repaint();
        repaint();
    }
    if (dyn != lastDyn)
    {
        lastDyn = dyn;
        for (auto* k : { &threshKnob, &rangeKnob, &attackKnob, &releaseKnob })
            k->setEnabled(dyn);
        scToggle.setEnabled(dyn);
    }
}

void ControlPanelComponent::paint(juce::Graphics& g)
{
    using namespace rackui;
    if (currentBandIndex < 0)
    {
        print(g, juce::String::fromUTF8 ("CLICK THE SCREEN TO ADD A BAND   \xc2\xb7   RIGHT-CLICK TO BYPASS   \xc2\xb7   DOUBLE-CLICK TO REMOVE"),
              getLocalBounds().toFloat(), Face::legend, 12.0f, 0.16f, colour::inkDim);
        return;
    }

    // Band window and section legends.
    const auto band = juce::Rectangle<float>(0.0f, 0.0f, 84.0f, 26.0f);
    oled(g, band);
    oledText(g, "BAND " + juce::String(currentBandIndex + 1).paddedLeft('0', 2), band, bandColour(), 12.0f,
             juce::Justification::centred, true);
    print(g, "FILTER", { 96.0f, 0.0f, 100.0f, 26.0f }, Face::legend, 11.5f, 0.22f, colour::ink, juce::Justification::centredLeft);
    const auto legend = [&](const char* text, float y, bool enabled)
    {
        print(g, text, { 0.0f, y, 48.0f, 18.0f }, Face::textMedium, 8.5f, 0.16f,
              colour::inkDim.withMultipliedAlpha(enabled ? 1.0f : 0.5f), juce::Justification::centredLeft);
    };
    legend("TYPE", 40.0f, true);
    legend("SLOPE", (float) slopeSelect.getY(), lastSlope);
    legend("ROUTE", (float) routeSelect.getY(), true);
    print(g, "dB/OCT", { (float) slopeSelect.getRight() + 2.0f, (float) slopeSelect.getY(), 38.0f, 18.0f }, Face::textMedium, 8.0f, 0.12f,
          colour::inkDim.withMultipliedAlpha(lastSlope ? 1.0f : 0.5f), juce::Justification::centredLeft);

    print(g, "DYNAMICS", { (float) dividerX[1] + 20.0f, 0.0f, 120.0f, 20.0f }, Face::legend, 11.5f, 0.22f, colour::ink,
          juce::Justification::centredLeft);

    for (int i = 0; i < 2; ++i)
        divider(g, (float) dividerX[(size_t) i], 4.0f, (float) getHeight() - 8.0f);
}

void ControlPanelComponent::resized()
{
    const int h = getHeight();
    const int filterW = 236;

    // Filter section.
    typeSelect.setBounds(48, 36, filterW - 48, 64);
    slopeSelect.setBounds(48, 108, 164, 18);   // leaves room for "dB/OCT" before the divider
    routeSelect.setBounds(48, 132, filterW - 48, 18);
    activeToggle.setBounds(0, 160, 90, 18);
    bypassToggle.setBounds(100, 160, 90, 18);

    // Band knobs, centred in the middle section.
    dividerX[0] = filterW + 18;
    int x = dividerX[0] + 14;
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        const auto s = k->preferredSize();
        k->setBounds(x, (h - s.y) / 2, s.x, s.y);
        x += s.x;
    }
    dividerX[1] = x + 14;

    // Dynamics: header row of switches, then four knobs.
    x = dividerX[1] + 20;
    dynToggle.setBounds(x + 100, 1, 46, 18);
    scToggle.setBounds(x + 154, 1, 70, 18);
    const int pitch = (getWidth() - x) / 4;
    int kx = x - 14;
    for (auto* k : { &threshKnob, &rangeKnob, &attackKnob, &releaseKnob })
    {
        const auto s = k->preferredSize();
        k->setBounds(kx + (pitch - s.x) / 2, 30, s.x, s.y);
        kx += pitch;
    }
}

} // namespace AxisEQ
