#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "SpectrumAnalyzer.h"

namespace ProEQ
{

/// FabFilter Pro-Q style interactive EQ canvas.
///
/// Interaction model:
///   - Only ACTIVE bands render visible nodes.
///   - Left-click on empty space  → activates the next free band slot at
///     that frequency/gain position (adds a band).
///   - Left-drag a node           → moves its frequency (X) and gain (Y).
///   - Mouse-wheel over a node    → adjusts Q (logarithmically).
///   - Right-click a node         → context menu: Bypass / Remove band.
///   - Clicking elsewhere deselects.
class EQCurveComponent : public juce::Component, public juce::Timer
{
public:
    EQCurveComponent(ProEQAudioProcessor& p, SpectrumAnalyzer& analyzer);
    ~EQCurveComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseMove        (const juce::MouseEvent& e) override;
    void mouseDown        (const juce::MouseEvent& e) override;
    void mouseDrag        (const juce::MouseEvent& e) override;
    void mouseUp          (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove   (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    /// Fired when a node becomes selected (or -1 when deselected).
    std::function<void(int)> onNodeSelected;

    // ── Coordinate helpers (also used by ControlPanelComponent) ──────────
    static float getPositionForFrequency(float freq,  float width);
    static float getFrequencyForPosition(float x,     float width);
    static float getPositionForGain     (float gain,  float height);
    static float getGainForPosition     (float y,     float height);

private:
    ProEQAudioProcessor& processor;
    SpectrumAnalyzer&    analyzer;

    // ── Drawing ───────────────────────────────────────────────────────────
    void drawGrid        (juce::Graphics& g, juce::Rectangle<float> b);
    void drawSpectrum    (juce::Graphics& g, juce::Rectangle<float> b);
    void drawResponseCurve(juce::Graphics& g, juce::Rectangle<float> b);
    void drawNodes       (juce::Graphics& g, juce::Rectangle<float> b);
    void drawAddCursor   (juce::Graphics& g, juce::Rectangle<float> b);

    double getCombinedMagnitudeResponse(double frequency);

    // ── Interaction helpers ────────────────────────────────────────────────
    /// Returns the index of the active band whose node is within hitRadius
    /// of (mx, my), or -1 if none.
    int hitTestNode(float mx, float my) const;

    /// Activates the first unused band slot and places it at (freq, gain).
    /// Returns the band index or -1 if all bands are taken.
    int addBandAt(float freq, float gain);

    void setParam(const char* suffix, int bandIndex, float value);
    void setChoiceParam(const char* suffix, int bandIndex, int index);
    void setBoolParam(const char* suffix, int bandIndex, bool on);

    // ── State ─────────────────────────────────────────────────────────────
    int draggingNodeIndex = -1;
    int hoveredNodeIndex  = -1;
    int selectedNodeIndex = -1;

    static constexpr float NodeRadius    = 7.0f;
    static constexpr float HitRadius     = 14.0f;  // pixels for hover/click
    static constexpr float GainScale     = 30.0f;  // ±30 dB visible range

    // Spectrum smoothing buffers
    std::array<float, FFTSize / 2> lastInputSpectrum;
    std::array<float, FFTSize / 2> lastOutputSpectrum;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQCurveComponent)
};

} // namespace ProEQ
