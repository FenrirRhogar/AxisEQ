#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "DSP/BandDesign.h"
#include <vector>

namespace AxisEQ
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
class EQCurveComponent : public juce::Component, public juce::Timer,
                         private juce::AudioProcessorParameter::Listener
{
public:
    EQCurveComponent(AxisEQAudioProcessor& p, SpectrumAnalyzer& analyzer);
    ~EQCurveComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseMove        (const juce::MouseEvent& e) override;
    void mouseExit        (const juce::MouseEvent& e) override;
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
    AxisEQAudioProcessor& processor;
    SpectrumAnalyzer&    analyzer;

    // ── Drawing ───────────────────────────────────────────────────────────
    void drawBackground  (juce::Graphics& g, juce::Rectangle<float> b);
    void drawGrid        (juce::Graphics& g, juce::Rectangle<float> b);
    void drawSpectrum    (juce::Graphics& g, juce::Rectangle<float> b);
    void drawResponseCurve(juce::Graphics& g, juce::Rectangle<float> b);
    void drawNodes       (juce::Graphics& g, juce::Rectangle<float> b);
    void drawAddCursor   (juce::Graphics& g, juce::Rectangle<float> b);

    double getSampleRateForCurve() const;
    std::vector<BandDesign> designActiveBands() const;
    double getCombinedMagnitudeResponse(const std::vector<BandDesign>& designs, double frequency) const;

    // ── Interaction helpers ────────────────────────────────────────────────
    /// Returns the index of the active band whose node is within hitRadius
    /// of (mx, my), or -1 if none.
    int hitTestNode(float mx, float my) const;

    /// Hover state and cursor from a mouse position (mouse events only; asking the OS
    /// for the pointer every frame is a round trip to the X server on Linux).
    void updateHover(juce::Point<float> pos);

    /// Activates the first unused band slot and places it at (freq, gain).
    /// Returns the band index or -1 if all bands are taken.
    int addBandAt(float freq, float gain);

    void setParam(const char* suffix, int bandIndex, float value);
    void setChoiceParam(const char* suffix, int bandIndex, int index);
    void setBoolParam(const char* suffix, int bandIndex, bool on);

    // Redraws only when something on screen moved: a parameter, the spectrum or a dynamic band.
    // Any thread may change a parameter, so the listener just raises a flag for the timer.
    void parameterValueChanged(int, float) override { parametersChanged = true; }
    void parameterGestureChanged(int, bool) override {}
    std::atomic<bool> parametersChanged { true };
    std::array<float, MaxBands> lastDynamicGain {};

    // ── State ─────────────────────────────────────────────────────────────
    int draggingNodeIndex = -1;
    int hoveredNodeIndex  = -1;
    int selectedNodeIndex = -1;

    static constexpr float NodeRadius    = 7.0f;
    static constexpr float HitRadius     = 14.0f;  // pixels for hover/click
    static constexpr float GainScale     = 30.0f;  // ±30 dB visible range

    // The glass, glow and grid never change between frames, so they are rendered once per size
    // and display scale; filling them live costs ~12 ms a frame in a large window.
    juce::Image background;
    float backgroundScale = 0.0f;

    // Spectrum smoothing buffers
    std::array<float, FFTSize / 2> lastInputSpectrum;
    std::array<float, FFTSize / 2> lastOutputSpectrum;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQCurveComponent)
};

} // namespace AxisEQ
