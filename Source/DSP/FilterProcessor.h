#pragma once

#include <JuceHeader.h>
#include "Utilities/Constants.h"
#include "FilterBand.h"
#include <array>

namespace ProEQ
{

/// A real-time safe biquad filter using Transposed Direct Form II
class Biquad
{
public:
    Biquad() = default;

    /// Sets the coefficients (should already be normalized so a0 = 1.0)
    void setCoefficients(double newB0, double newB1, double newB2, double newA1, double newA2) noexcept
    {
        b0 = static_cast<float>(newB0);
        b1 = static_cast<float>(newB1);
        b2 = static_cast<float>(newB2);
        a1 = static_cast<float>(newA1);
        a2 = static_cast<float>(newA2);
    }
    
    void reset() noexcept
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    float processSample(float in) noexcept
    {
        const float out = in * b0 + z1;
        z1 = in * b1 - out * a1 + z2;
        z2 = in * b2 - out * a2;
        return out;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

/// Handles a single EQ band, which can consist of multiple cascaded biquads for steeper slopes.
class CascadedBiquad
{
public:
    CascadedBiquad() = default;

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }
    
    void reset() noexcept
    {
        for (auto& bq : biquads)
            bq.reset();
    }
    
    /// Recalculates coefficients based on current APVTS values from the given band.
    void updateParameters(const FilterBandParameters& params) noexcept;
    
    inline float processSample(float in) noexcept
    {
        if (! isActive)
            return in;

        float out = in;
        for (int i = 0; i < numStages; ++i)
            out = biquads[i].processSample(out);
            
        return out;
    }

private:
    std::array<Biquad, 8> biquads;
    int numStages = 1;
    bool isActive = true;
    double sampleRate = 44100.0;
};

/// The main EQ filter chain. Processes a single channel through all 24 bands.
class FilterProcessor
{
public:
    FilterProcessor() = default;

    void prepare(double sampleRate) noexcept
    {
        for (auto& band : bands)
            band.prepare(sampleRate);
    }
    
    void reset() noexcept
    {
        for (auto& band : bands)
            band.reset();
    }
    
    /// Updates all bands' coefficients
    void updateParameters(const std::array<FilterBandParameters, MaxBands>& allParams) noexcept
    {
        for (int i = 0; i < MaxBands; ++i)
        {
            bands[i].updateParameters(allParams[i]);
        }
    }
    
    /// Updates a single band's coefficients
    void updateBandParameters(int bandIndex, const FilterBandParameters& params) noexcept
    {
        jassert(bandIndex >= 0 && bandIndex < MaxBands);
        if (bandIndex >= 0 && bandIndex < MaxBands)
        {
            bands[bandIndex].updateParameters(params);
        }
    }
    
    /// Processes a single sample through the 24-band chain
    inline float processSample(float sample) noexcept
    {
        float out = sample;
        for (auto& band : bands)
        {
            out = band.processSample(out);
        }
        return out;
    }
    
    /// Processes a block of samples (single channel)
    void processBlock(float* channelData, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = processSample(channelData[i]);
        }
    }

private:
    std::array<CascadedBiquad, MaxBands> bands;
};

} // namespace ProEQ
