#pragma once

#include <JuceHeader.h>
#include "DSP/BandDesign.h"
#include "DSP/FilterBand.h"
#include <atomic>

namespace AxisEQ
{

/// One EQ band for up to two channels.
///
/// - Frequency, gain and Q are smoothed, and coefficients are redesigned every
///   UpdateInterval samples, so moving a band never zippers.
/// - Switching the band in or out, or changing its type, slope or routing, crossfades
///   against the unprocessed signal, so it never clicks.
/// - The dynamic stage listens through a detector matched to the band (band-pass for
///   bells, low-pass for low shelves, high-pass for high shelves), so it only reacts to
///   its own frequency range. In stereo routing both channels share one envelope.
class BandProcessor
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /// Processes this band in place. @p right is null for mono. @p scLeft / @p scRight carry
    /// the external sidechain and are null when none is connected.
    void process(float* left, float* right, int numSamples,
                 const float* scLeft, const float* scRight,
                 const FilterBandParameters& params) noexcept;

    /// Current dynamic gain in dB (0 when static), for the gain-reduction display.
    float getDynamicGainDb() const noexcept { return dynamicGainForGui.load(std::memory_order_relaxed); }

private:
    static constexpr int UpdateInterval = 16;
    static constexpr double Ratio = 2.0;
    static constexpr double KneeDb = 6.0;

    struct Structure
    {
        FilterType type = FilterType::Bell;
        FilterSlope slope = FilterSlope::dB12;
        RoutingMode route = RoutingMode::Stereo;

        bool operator!=(const Structure& other) const noexcept
        {
            return type != other.type || slope != other.slope || route != other.route;
        }
    };

    /// Cascaded biquads (transposed direct form II) with state for two channels.
    struct Cascade
    {
        BandDesign design;
        double z1[MaxStages][2] {}, z2[MaxStages][2] {};

        void clear() noexcept;
        float process(int channel, float input) noexcept;
    };

    void adopt(const Structure& next) noexcept;
    void redesign(double frequency, double q, double gainDb) noexcept;
    double dynamicGainFor(double levelDb, double thresholdDb, double rangeDb) const noexcept;

    double sampleRate = 44100.0;
    Structure structure;
    bool pendingStructure = false;
    Structure nextStructure;

    Cascade filter, detector;
    bool hasDetector = false;

    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> frequency { 1000.0 }, q { 0.707 };
    juce::SmoothedValue<double> gainDb, dynamicAmount;
    juce::SmoothedValue<float> mix;   // 0 = band out, 1 = band in
    bool needsDesign = true;

    double envelope = 0.0;            // linear peak level of the detector output
    double dynamicGainDb = 0.0;       // last computed dynamic gain, applied at the next redesign
    std::atomic<float> dynamicGainForGui { 0.0f };
};

} // namespace AxisEQ
