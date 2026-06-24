#include "EQCurveComponent.h"
#include "../DSP/FilterProcessor.h"
#include <complex>

namespace ProEQ
{

//──────────────────────────────────────────────────────────────────────────────
// Colours
//──────────────────────────────────────────────────────────────────────────────
namespace
{
    juce::Colour BG_EQ   { 0xFF0D0D1A };
    juce::Colour GRID    { 0x18FFFFFF };
    juce::Colour GRID_0  { 0x44FFFFFF };   // 0 dB line
    juce::Colour LABEL   { 0x88FFFFFF };
    juce::Colour CURVE   { 0xFFFFFFFF };
    juce::Colour FILL    { 0x12FFFFFF };
    juce::Colour SPEC_IN { 0x2C3D84FF };
    juce::Colour SPEC_OUT{ 0x3800E676 };
    juce::Colour ADD_HINT{ 0x55FFFFFF };   // ghost cursor when no hover
}

//──────────────────────────────────────────────────────────────────────────────
EQCurveComponent::EQCurveComponent(ProEQAudioProcessor& p, SpectrumAnalyzer& a)
    : processor(p), analyzer(a)
{
    lastInputSpectrum.fill(-100.0f);
    lastOutputSpectrum.fill(-100.0f);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    startTimerHz(60);
}

EQCurveComponent::~EQCurveComponent() { stopTimer(); }

//──────────────────────────────────────────────────────────────────────────────
// Timer — update hover, repaint
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::timerCallback()
{
    auto mousePos = getMouseXYRelative().toFloat();
    int newHover = hitTestNode(mousePos.x, mousePos.y);

    if (newHover != hoveredNodeIndex)
    {
        hoveredNodeIndex = newHover;
        setMouseCursor(newHover >= 0 ? juce::MouseCursor::PointingHandCursor
                                     : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
    repaint();
}

//──────────────────────────────────────────────────────────────────────────────
// paint
//──────────────────────────────────────────────────────────────────────────────
void EQCurveComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour(BG_EQ);
    g.fillRect(b);

    drawGrid         (g, b);
    drawSpectrum     (g, b);
    drawResponseCurve(g, b);
    drawNodes        (g, b);

    // Ghost crosshair when hovering empty space (add-band hint)
    if (hoveredNodeIndex < 0 && isMouseOver(false))
        drawAddCursor(g, b);
}

void EQCurveComponent::resized() {}

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
        g.setFont(9.5f);
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
            g.setFont(9.5f);
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
    std::array<float, FFTSize / 2> curIn, curOut;
    analyzer.getMagnitudes(0, false, curIn);
    analyzer.getMagnitudes(0, true,  curOut);

    for (size_t i = 0; i < curIn.size(); ++i)
    {
        lastInputSpectrum [i] += (curIn [i] - lastInputSpectrum [i]) * 0.25f;
        lastOutputSpectrum[i] += (curOut[i] - lastOutputSpectrum[i]) * 0.25f;
    }

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
            float y = juce::jmap(spec[i], -100.f, 12.f, b.getBottom(), b.getY());
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

    for (int px = 0; px < static_cast<int>(w); ++px)
    {
        float freq  = getFrequencyForPosition(static_cast<float>(px), w);
        double mag  = getCombinedMagnitudeResponse(static_cast<double>(freq));
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

    g.setColour(CURVE);
    g.strokePath(curve, juce::PathStrokeType(1.8f,
        juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
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

        // Place the new band
        setParam       ("freq",  i, freq);
        setParam       ("gain",  i, gain);
        setParam       ("q",     i, 1.0f);
        setChoiceParam ("type",  i, 0);    // Bell
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
void EQCurveComponent::mouseMove(const juce::MouseEvent& /*e*/)
{
    repaint(); // redraws the add-band cursor / tooltip
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

void EQCurveComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
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
double EQCurveComponent::getCombinedMagnitudeResponse(double frequency)
{
    double sr = processor.getSampleRate();
    if (sr <= 0.0) sr = 44100.0;

    double w = 2.0 * juce::MathConstants<double>::pi * frequency / sr;
    std::complex<double> z(std::cos(w), std::sin(w));
    std::complex<double> total(1.0, 0.0);

    for (int i = 0; i < MaxBands; ++i)
    {
        if (!processor.bandParams[i].shouldProcess()) continue;

        double A      = std::pow(10.0, processor.bandParams[i].getGain() / 40.0);
        double w0     = 2.0 * juce::MathConstants<double>::pi
                            * processor.bandParams[i].getFrequency() / sr;
        double cos_w0 = std::cos(w0);
        double sin_w0 = std::sin(w0);
        double alpha  = sin_w0 / (2.0 * std::max(0.01f, processor.bandParams[i].getQ()));

        double b0=1, b1=0, b2=0, a0=1, a1=0, a2=0;
        switch (processor.bandParams[i].getFilterType())
        {
            case FilterType::Bell:
                b0=1+alpha*A; b1=-2*cos_w0; b2=1-alpha*A;
                a0=1+alpha/A; a1=-2*cos_w0; a2=1-alpha/A; break;
            case FilterType::LowCut:
                b0=(1+cos_w0)/2; b1=-(1+cos_w0); b2=(1+cos_w0)/2;
                a0=1+alpha;      a1=-2*cos_w0;    a2=1-alpha; break;
            case FilterType::HighCut:
                b0=(1-cos_w0)/2; b1=1-cos_w0;  b2=(1-cos_w0)/2;
                a0=1+alpha;      a1=-2*cos_w0;  a2=1-alpha; break;
            case FilterType::LowShelf:
                b0=A*((A+1)-(A-1)*cos_w0+2*std::sqrt(A)*alpha);
                b1=2*A*((A-1)-(A+1)*cos_w0);
                b2=A*((A+1)-(A-1)*cos_w0-2*std::sqrt(A)*alpha);
                a0=(A+1)+(A-1)*cos_w0+2*std::sqrt(A)*alpha;
                a1=-2*((A-1)+(A+1)*cos_w0);
                a2=(A+1)+(A-1)*cos_w0-2*std::sqrt(A)*alpha; break;
            case FilterType::HighShelf:
                b0=A*((A+1)+(A-1)*cos_w0+2*std::sqrt(A)*alpha);
                b1=-2*A*((A-1)+(A+1)*cos_w0);
                b2=A*((A+1)+(A-1)*cos_w0-2*std::sqrt(A)*alpha);
                a0=(A+1)-(A-1)*cos_w0+2*std::sqrt(A)*alpha;
                a1=2*((A-1)-(A+1)*cos_w0);
                a2=(A+1)-(A-1)*cos_w0-2*std::sqrt(A)*alpha; break;
            case FilterType::Notch:
                b0=1; b1=-2*cos_w0; b2=1;
                a0=1+alpha; a1=-2*cos_w0; a2=1-alpha; break;
            case FilterType::BandPass:
                b0=alpha; b1=0; b2=-alpha;
                a0=1+alpha; a1=-2*cos_w0; a2=1-alpha; break;
            case FilterType::TiltShelf:
            {
                double th = std::tan(juce::MathConstants<double>::pi
                                     * processor.bandParams[i].getFrequency() / sr);
                b0=th+A; b1=th-A; b2=0;
                a0=th*A+1; a1=th*A-1; a2=0; break;
            }
        }

        std::complex<double> invZ  = std::pow(z, -1);
        std::complex<double> invZ2 = std::pow(z, -2);
        std::complex<double> num   = b0 + b1 * invZ + b2 * invZ2;
        std::complex<double> den   = a0 + a1 * invZ + a2 * invZ2;
        std::complex<double> H     = num / den;

        int stages = processor.bandParams[i].filterUsesSlope()
                         ? biquadStagesForSlope(processor.bandParams[i].getSlope()) : 1;
        for (int s = 0; s < stages; ++s)
            total *= H;
    }
    return std::abs(total);
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

} // namespace ProEQ
