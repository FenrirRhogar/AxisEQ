#include "EQCurveComponent.h"
#include "Theme.h"
#include <complex>

namespace AxisEQ
{

//──────────────────────────────────────────────────────────────────────────────
// Colours
//──────────────────────────────────────────────────────────────────────────────
namespace
{
    // Glass display in the fenrikos style, tinted with the AxisEQ accent.
    const juce::Colour BG_EQ   = Theme::glass;
    const juce::Colour GRID    = Theme::accent.withAlpha(0.09f);
    const juce::Colour GRID_0  = Theme::accent.withAlpha(0.30f);   // 0 dB line
    const juce::Colour LABEL   = Theme::accent.withAlpha(0.55f);
    const juce::Colour CURVE   = Theme::accent.brighter(0.6f);
    const juce::Colour FILL    = Theme::accent.withAlpha(0.10f);
    const juce::Colour SPEC_IN = Theme::accent.withAlpha(0.12f);
    const juce::Colour SPEC_OUT= Theme::accent.withAlpha(0.28f);
    const juce::Colour ADD_HINT= Theme::accent.withAlpha(0.45f);   // ghost cursor when no hover
}

//──────────────────────────────────────────────────────────────────────────────
EQCurveComponent::EQCurveComponent(AxisEQAudioProcessor& p, SpectrumAnalyzer& a)
    : processor(p), analyzer(a)
{
    lastInputSpectrum.fill(-100.0f);
    lastOutputSpectrum.fill(-100.0f);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setOpaque(true);   // the cached background repaints the card corners, so nothing behind needs redrawing
    for (auto* param : processor.getParameters())
        param->addListener(this);
    startTimerHz(60);
}

EQCurveComponent::~EQCurveComponent()
{
    stopTimer();
    for (auto* param : processor.getParameters())
        param->removeListener(this);
}

//──────────────────────────────────────────────────────────────────────────────
// Timer — repaint when something on screen moved
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::timerCallback()
{
    bool changed = parametersChanged.exchange(false);

    std::array<float, FFTSize / 2> curIn, curOut;
    analyzer.getMagnitudes(0, false, curIn);
    analyzer.getMagnitudes(0, true,  curOut);

    // Levels under -90 dB are drawn at the floor, so movement down there is invisible.
    const auto smooth = [&changed](float& shown, float target)
    {
        const float before = juce::jmax(shown, -90.0f);
        shown += (target - shown) * 0.25f;
        changed = changed || std::abs(juce::jmax(shown, -90.0f) - before) > 0.01f;
    };
    for (size_t i = 0; i < curIn.size(); ++i)
    {
        smooth(lastInputSpectrum [i], curIn [i]);
        smooth(lastOutputSpectrum[i], curOut[i]);
    }

    for (int i = 0; i < MaxBands; ++i)
    {
        const auto& band = processor.bandParams[i];
        const float dyn = band.isActive() && band.isDynEnabled() && band.filterUsesGain() ? processor.getDynamicGainDb(i) : 0.0f;
        changed = changed || std::abs(dyn - lastDynamicGain[static_cast<size_t>(i)]) > 0.01f;
        lastDynamicGain[static_cast<size_t>(i)] = dyn;
    }

    if (changed)
        repaint();
}

void EQCurveComponent::updateHover(juce::Point<float> pos)
{
    const int newHover = isMouseOver() ? hitTestNode(pos.x, pos.y) : -1;
    if (newHover != hoveredNodeIndex)
    {
        hoveredNodeIndex = newHover;
        setMouseCursor(newHover >= 0 ? juce::MouseCursor::PointingHandCursor
                                     : juce::MouseCursor::CrosshairCursor);
    }
}

//──────────────────────────────────────────────────────────────────────────────
// paint
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (background.isNull() || scale != backgroundScale)
    {
        backgroundScale = scale;
        background = juce::Image(juce::Image::RGB, juce::jmax(1, juce::roundToInt(b.getWidth() * scale)),
                                 juce::jmax(1, juce::roundToInt(b.getHeight() * scale)), false);
        juce::Graphics bg(background);
        bg.addTransform(juce::AffineTransform::scale(scale));
        drawBackground(bg, b);
    }
    g.drawImage(background, b);

    juce::Path screen;
    screen.addRoundedRectangle(b, 8.0f);
    g.reduceClipRegion(screen);

    drawSpectrum     (g, b);
    drawResponseCurve(g, b);
    drawNodes        (g, b);

    // Ghost crosshair when hovering empty space (add-band hint)
    if (hoveredNodeIndex < 0 && isMouseOver(false))
        drawAddCursor(g, b);
}

void EQCurveComponent::resized() { background = {}; }

//──────────────────────────────────────────────────────────────────────────────
// Background — glass, glow and grid, cached in `background`
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::drawBackground(juce::Graphics& g, juce::Rectangle<float> b)
{
    // The corners show the card this sits in (Theme::card, 6 px larger on each side in the editor).
    g.setGradientFill(juce::ColourGradient(Theme::cardFill.brighter(0.07f), 0.0f, b.getY() - 6.0f,
                                           Theme::cardFill, 0.0f, b.getBottom() + 6.0f, false));
    g.fillRect(b);

    juce::Path screen;
    screen.addRoundedRectangle(b, 8.0f);
    g.reduceClipRegion(screen);

    g.setColour(BG_EQ);
    g.fillRect(b);
    g.setGradientFill(juce::ColourGradient(Theme::accent.withAlpha(0.10f), b.getCentreX(), b.getBottom(),
                                           Theme::accent.withAlpha(0.0f), b.getCentreX(), b.getY(), true));
    g.fillRect(b);

    drawGrid(g, b);
}

//──────────────────────────────────────────────────────────────────────────────
// Grid
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> b)
{
    const float freqs[] = { 20, 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : freqs)
    {
        float x = getPositionForFrequency(f, b.getWidth());
        g.setColour(GRID);
        g.drawVerticalLine(static_cast<int>(x), b.getY(), b.getBottom());

        g.setColour(LABEL);
        g.setFont(Theme::mono(10.0f));
        juce::String txt = f >= 1000.f ? (juce::String(f / 1000.f, f >= 10000.f ? 0 : 1) + "k") : juce::String(static_cast<int>(f));
        g.drawText(txt, static_cast<int>(x) + 3, static_cast<int>(b.getBottom()) - 14, 32, 12,
                   juce::Justification::centredLeft, false);
    }

    const float gains[] = { -24, -18, -12, -6, 0, 6, 12, 18, 24 };
    for (float gv : gains)
    {
        float y = getPositionForGain(gv, b.getHeight());
        g.setColour(gv == 0.f ? GRID_0 : GRID);
        g.drawHorizontalLine(static_cast<int>(y), b.getX(), b.getRight());

        if (gv != 0.f)
        {
            g.setColour(LABEL);
            g.setFont(Theme::mono(10.0f));
            g.drawText((gv > 0 ? "+" : "") + juce::String(static_cast<int>(gv)),
                       4, static_cast<int>(y) - 12, 28, 12, juce::Justification::centredLeft, false);
        }
    }
}

//──────────────────────────────────────────────────────────────────────────────
// Spectrum
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> b)
{
    auto makePath = [&](const std::array<float, FFTSize / 2>& spec) -> juce::Path
    {
        juce::Path p;
        bool started = false;
        double sr       = processor.getSampleRate();
        double binWidth = sr / FFTSize;

        for (size_t i = 1; i < spec.size(); ++i)
        {
            double freq = i * binWidth;
            if (freq < MinFrequency || freq > MaxFrequency) continue;

            float x = getPositionForFrequency(static_cast<float>(freq), b.getWidth());
            float y = juce::jmap(spec[i], -90.f, 0.f, b.getBottom(), b.getY());
            y = juce::jlimit(b.getY(), b.getBottom(), y);

            if (!started) { p.startNewSubPath(x, b.getBottom()); p.lineTo(x, y); started = true; }
            else            p.lineTo(x, y);
        }
        p.lineTo(b.getRight(), b.getBottom());
        p.closeSubPath();
        return p;
    };

    g.setColour(SPEC_IN);
    g.fillPath(makePath(lastInputSpectrum));

    g.setGradientFill(juce::ColourGradient(
        SPEC_OUT, b.getX(), b.getBottom(),
        SPEC_OUT.withAlpha(0.0f), b.getX(), b.getY(), false));
    g.fillPath(makePath(lastOutputSpectrum));
}

//──────────────────────────────────────────────────────────────────────────────
// Response curve
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::drawResponseCurve(juce::Graphics& g, juce::Rectangle<float> b)
{
    juce::Path curve;
    bool started = false;
    const float w = b.getWidth();
    const auto designs = designActiveBands();

    for (int px = 0; px < static_cast<int>(w); ++px)
    {
        float freq  = getFrequencyForPosition(static_cast<float>(px), w);
        double mag  = getCombinedMagnitudeResponse(designs, static_cast<double>(freq));
        float  db   = static_cast<float>(juce::Decibels::gainToDecibels(mag, -100.0));
        float  y    = getPositionForGain(db, b.getHeight());

        if (!started) { curve.startNewSubPath(static_cast<float>(px), y); started = true; }
        else            curve.lineTo(static_cast<float>(px), y);
    }

    // Subtle fill between curve and 0 dB
    juce::Path fill = curve;
    float zeroY = getPositionForGain(0.f, b.getHeight());
    fill.lineTo(w, zeroY);
    fill.lineTo(0.f, zeroY);
    fill.closeSubPath();
    g.setColour(FILL);
    g.fillPath(fill);

    Theme::glow(g, curve, CURVE, 2.0f);
}

//──────────────────────────────────────────────────────────────────────────────
// Nodes — only active bands
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::drawNodes(juce::Graphics& g, juce::Rectangle<float> b)
{
    for (int i = 0; i < MaxBands; ++i)
    {
        if (!processor.bandParams[i].isActive()) continue;

        bool bypassed  = processor.bandParams[i].isBypassed();
        float freq     = processor.bandParams[i].getFrequency();
        float gain     = processor.bandParams[i].filterUsesGain()
                             ? processor.bandParams[i].getGain() : 0.0f;

        float x = getPositionForFrequency(freq, b.getWidth());
        float y = getPositionForGain(gain, b.getHeight());

        bool isHov = (i == hoveredNodeIndex);
        bool isSel = (i == selectedNodeIndex);
        float r    = NodeRadius + (isHov || isSel ? 2.f : 0.f);

        juce::Colour col = bypassed
                            ? juce::Colours::grey.withAlpha(0.6f)
                            : BandColours[static_cast<size_t>(i)];

        // Dynamic Gain feedback
        float dynGain = 0.0f;
        if (processor.bandParams[i].isDynEnabled() && processor.bandParams[i].filterUsesGain())
            dynGain = processor.getDynamicGainDb(i);

        if (std::abs(dynGain) > 0.01f && !bypassed)
        {
            float yDyn = getPositionForGain(gain + dynGain, b.getHeight());
            
            // Draw a vertical line from static node to dynamic node
            g.setColour(col.withAlpha(0.5f));
            g.drawLine(x, y, x, yDyn, 2.0f);

            // Draw dynamic ring
            g.setColour(col);
            g.drawEllipse(x - r, yDyn - r, r * 2.f, r * 2.f, 2.0f);
            
            // Draw a subtle fill
            g.setColour(col.withAlpha(0.3f));
            g.fillEllipse(x - r, yDyn - r, r * 2.f, r * 2.f);
        }

        // Glow halo
        g.setColour(col.withAlpha(0.15f));
        g.fillEllipse(x - r - 5.f, y - r - 5.f, (r + 5.f) * 2.f, (r + 5.f) * 2.f);

        // Node fill
        g.setColour(col.withAlpha(bypassed ? 0.4f : 0.85f));
        g.fillEllipse(x - r, y - r, r * 2.f, r * 2.f);

        // Border
        g.setColour(isSel ? juce::Colours::white : col.brighter(0.5f));
        g.drawEllipse(x - r, y - r, r * 2.f, r * 2.f, isSel ? 2.f : 1.f);

        // Band-number label inside node
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(8.5f);
        g.drawText(juce::String(i + 1), static_cast<int>(x - r), static_cast<int>(y - r),
                   static_cast<int>(r * 2.f), static_cast<int>(r * 2.f),
                   juce::Justification::centred, false);
    }
}

//──────────────────────────────────────────────────────────────────────────────
// Ghost cursor (add-band hint)
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::drawAddCursor(juce::Graphics& g, juce::Rectangle<float> b)
{
    auto mp = getMouseXYRelative().toFloat();
    float freq = getFrequencyForPosition(mp.x, b.getWidth());
    float gain = getGainForPosition(mp.y, b.getHeight());

    // Show a "+" hint circle
    float r = NodeRadius;
    g.setColour(ADD_HINT);
    g.drawEllipse(mp.x - r, mp.y - r, r * 2.f, r * 2.f, 1.5f);
    g.setColour(ADD_HINT.withAlpha(0.5f));
    g.drawLine(mp.x, mp.y - r - 4.f, mp.x, mp.y + r + 4.f, 1.f);
    g.drawLine(mp.x - r - 4.f, mp.y, mp.x + r + 4.f, mp.y, 1.f);

    // Frequency readout
    juce::String freqTxt = freq >= 1000.f
        ? juce::String(freq / 1000.f, 2) + " kHz"
        : juce::String(static_cast<int>(freq)) + " Hz";
    juce::String gainTxt = (gain >= 0 ? "+" : "") + juce::String(gain, 1) + " dB";

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(11.f);
    float tx = juce::jlimit(2.f, b.getWidth() - 80.f, mp.x + 12.f);
    float ty = mp.y > b.getHeight() * 0.8f ? mp.y - 30.f : mp.y + 10.f;
    g.drawText(freqTxt + "  " + gainTxt,
               static_cast<int>(tx), static_cast<int>(ty), 120, 16,
               juce::Justification::centredLeft, false);
}

//──────────────────────────────────────────────────────────────────────────────
// Hit-test — only active bands
//──────────────────────────────────────────────────────────────────────────────
int EQCurveComponent::hitTestNode(float mx, float my) const
{
    auto b = getLocalBounds().toFloat();
    // Iterate in reverse so higher-indexed (drawn-on-top) bands are preferred
    for (int i = MaxBands - 1; i >= 0; --i)
    {
        if (!processor.bandParams[i].isActive()) continue;

        float freq = processor.bandParams[i].getFrequency();
        float gain = processor.bandParams[i].filterUsesGain()
                         ? processor.bandParams[i].getGain() : 0.f;

        float nx = getPositionForFrequency(freq, b.getWidth());
        float ny = getPositionForGain(gain, b.getHeight());

        float dx = mx - nx, dy = my - ny;
        if (dx * dx + dy * dy <= HitRadius * HitRadius)
            return i;
    }
    return -1;
}

//──────────────────────────────────────────────────────────────────────────────
// Add a new band at the given frequency/gain
//──────────────────────────────────────────────────────────────────────────────
int EQCurveComponent::addBandAt(float freq, float gain)
{
    // Find first inactive slot
    for (int i = 0; i < MaxBands; ++i)
    {
        if (processor.bandParams[i].isActive()) continue;

        int type = 0; // Bell
        if (freq < 80.0f) type = 1;         // Low Cut
        else if (freq > 8000.0f) type = 2;  // High Cut

        // Place the new band
        setParam       ("freq",  i, freq);
        setParam       ("gain",  i, gain);
        setParam       ("q",     i, 1.0f);
        setChoiceParam ("type",  i, type);
        setChoiceParam ("slope", i, 1);    // 12 dB/oct
        setChoiceParam ("route", i, 0);    // Stereo
        setBoolParam   ("bypass",i, false);
        setBoolParam   ("active",i, true);
        return i;
    }
    return -1;
}



//──────────────────────────────────────────────────────────────────────────────
// Param helpers
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::setParam(const char* suffix, int idx, float value)
{
    if (auto* p = processor.apvts.getParameter(paramID(suffix, idx)))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

void EQCurveComponent::setChoiceParam(const char* suffix, int idx, int choice)
{
    if (auto* p = processor.apvts.getParameter(paramID(suffix, idx)))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(choice)));
}

void EQCurveComponent::setBoolParam(const char* suffix, int idx, bool on)
{
    if (auto* p = processor.apvts.getParameter(paramID(suffix, idx)))
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
}

//──────────────────────────────────────────────────────────────────────────────
// Mouse
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    updateHover(e.position);
    repaint(); // redraws the add-band cursor / tooltip
}

void EQCurveComponent::mouseExit(const juce::MouseEvent& e)
{
    updateHover(e.position);
    repaint();
}

void EQCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    auto b  = getLocalBounds().toFloat();
    int hit = hitTestNode(static_cast<float>(e.x), static_cast<float>(e.y));

    if (e.mods.isRightButtonDown())
    {
        // Right-click on a node → toggle bypass state
        if (hit >= 0)
        {
            bool isByp = processor.bandParams[hit].isBypassed();
            setBoolParam("bypass", hit, !isByp);

            // Select it if not already selected
            if (selectedNodeIndex != hit)
            {
                selectedNodeIndex = hit;
                if (onNodeSelected) onNodeSelected(hit);
            }
            repaint();
        }
        return;
    }

    if (hit >= 0)
    {
        // Select & start drag
        draggingNodeIndex = hit;
        selectedNodeIndex = hit;
        if (onNodeSelected) onNodeSelected(hit);

        if (auto* p = processor.apvts.getParameter(paramID("freq", hit)))
            p->beginChangeGesture();
        if (processor.bandParams[hit].filterUsesGain())
            if (auto* p = processor.apvts.getParameter(paramID("gain", hit)))
                p->beginChangeGesture();
    }
    else
    {
        // Click on empty space → add a new band
        float freq = getFrequencyForPosition(static_cast<float>(e.x), b.getWidth());
        float gain = getGainForPosition(static_cast<float>(e.y), b.getHeight());
        freq = juce::jlimit(MinFrequency, MaxFrequency, freq);
        gain = juce::jlimit(MinGain,      MaxGain,      gain);

        int newBand = addBandAt(freq, gain);
        if (newBand >= 0)
        {
            selectedNodeIndex = newBand;
            if (onNodeSelected) onNodeSelected(newBand);
        }
        else
        {
            selectedNodeIndex = -1;
            if (onNodeSelected) onNodeSelected(-1);
        }
    }

    repaint();
}

void EQCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingNodeIndex < 0) return;

    auto b    = getLocalBounds().toFloat();
    float freq = juce::jlimit(MinFrequency, MaxFrequency,
        getFrequencyForPosition(static_cast<float>(e.x), b.getWidth()));
    float gain = juce::jlimit(MinGain, MaxGain,
        getGainForPosition(static_cast<float>(e.y), b.getHeight()));

    setParam("freq", draggingNodeIndex, freq);

    if (processor.bandParams[draggingNodeIndex].filterUsesGain())
        setParam("gain", draggingNodeIndex, gain);
}

void EQCurveComponent::mouseUp(const juce::MouseEvent& e)
{
    updateHover(e.position);   // a click may have added a band under the pointer, a drag moved one
    if (draggingNodeIndex < 0) return;

    if (auto* p = processor.apvts.getParameter(paramID("freq", draggingNodeIndex)))
        p->endChangeGesture();
    if (processor.bandParams[draggingNodeIndex].filterUsesGain())
        if (auto* p = processor.apvts.getParameter(paramID("gain", draggingNodeIndex)))
            p->endChangeGesture();

    draggingNodeIndex = -1;
}

void EQCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    int hit = hitTestNode(static_cast<float>(e.x), static_cast<float>(e.y));
    if (hit < 0) return;

    // Double-click → permanently remove the band (deactivate + reset gain)
    setBoolParam("active", hit, false);
    setBoolParam("bypass", hit, false);
    setParam    ("gain",   hit, 0.f);
    updateHover(e.position);

    if (selectedNodeIndex == hit)
    {
        selectedNodeIndex = -1;
        if (onNodeSelected) onNodeSelected(-1);
    }
    repaint();
}

void EQCurveComponent::mouseWheelMove(const juce::MouseEvent& /*e*/,
                                       const juce::MouseWheelDetails& wheel)
{
    int hit = hoveredNodeIndex;
    if (hit < 0) return;

    float q    = processor.bandParams[hit].getQ();
    float qLog = std::log10(q) + wheel.deltaY * 1.5f;
    setParam("q", hit, std::clamp(std::pow(10.f, qLog), MinQ, MaxQ));
}

//──────────────────────────────────────────────────────────────────────────────
// Frequency response math
//──────────────────────────────────────────────────────────────────────────────
double EQCurveComponent::getSampleRateForCurve() const
{
    const double sr = processor.getSampleRate();
    return sr > 0.0 ? sr : 44100.0;
}

// Same filter design as the audio engine, so the curve shows exactly what is heard.
// Designed once per repaint, then evaluated at every pixel.
std::vector<BandDesign> EQCurveComponent::designActiveBands() const
{
    std::vector<BandDesign> designs;
    const double sr = getSampleRateForCurve();
    for (int i = 0; i < MaxBands; ++i)
    {
        const auto& band = processor.bandParams[static_cast<size_t>(i)];
        if (band.shouldProcess())
            designs.push_back(designBand(band.getFilterType(), band.getSlope(), band.getFrequency(),
                                         band.getQ(), band.getGain(), sr));
    }
    return designs;
}

double EQCurveComponent::getCombinedMagnitudeResponse(const std::vector<BandDesign>& designs, double frequency) const
{
    const double sr = getSampleRateForCurve();
    double total = 1.0;
    for (const auto& design : designs)
        total *= design.magnitude(frequency, sr);
    return total;
}

//──────────────────────────────────────────────────────────────────────────────
// Coordinate helpers
//──────────────────────────────────────────────────────────────────────────────
float EQCurveComponent::getPositionForFrequency(float freq, float width)
{
    float lo = std::log10(MinFrequency), hi = std::log10(MaxFrequency);
    return (std::log10(std::clamp(freq, MinFrequency, MaxFrequency)) - lo) / (hi - lo) * width;
}

float EQCurveComponent::getFrequencyForPosition(float x, float width)
{
    float lo = std::log10(MinFrequency), hi = std::log10(MaxFrequency);
    return std::pow(10.f, lo + std::clamp(x / width, 0.f, 1.f) * (hi - lo));
}

float EQCurveComponent::getPositionForGain(float gain, float height)
{
    return height * 0.5f - (gain / GainScale) * (height * 0.5f);
}

float EQCurveComponent::getGainForPosition(float y, float height)
{
    return (height * 0.5f - y) / (height * 0.5f) * GainScale;
}

} // namespace AxisEQ
