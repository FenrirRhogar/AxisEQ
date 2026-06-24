#include "FilterProcessor.h"
#include <cmath>
#include <algorithm>

namespace ProEQ
{

void CascadedBiquad::updateParameters(const FilterBandParameters& params) noexcept
{
    isActive = params.shouldProcess();
    if (! isActive)
        return;

    const auto type = params.getFilterType();
    const auto slope = params.getSlope();
    
    const double f0 = static_cast<double>(params.getFrequency());
    const double Q = static_cast<double>(params.getQ());
    
    // Some filters (like cut, notch, bandpass) ignore the gain parameter.
    // getGain() returns 0 if we don't care, or we can just read it anyway.
    const double gainDb = static_cast<double>(params.getGain());
    
    // Clamp frequency to slightly below Nyquist to prevent instability
    const double clampedF0 = std::clamp(f0, 20.0, sampleRate * 0.49);
    
    const double w0 = juce::MathConstants<double>::twoPi * clampedF0 / sampleRate;
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    
    // Add small epsilon to Q to prevent division by zero, though UI should prevent it
    const double safeQ = std::max(Q, 0.01);
    const double alpha = sin_w0 / (2.0 * safeQ);
    
    // For peaking and shelving filters, A is related to gain
    const double A = std::pow(10.0, gainDb / 40.0);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    numStages = 1; // Default to 1 stage unless it's a cut filter with a steeper slope

    switch (type)
    {
        case FilterType::Bell:
        {
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cos_w0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cos_w0;
            a2 = 1.0 - alpha / A;
            break;
        }
        case FilterType::LowCut:
        {
            numStages = params.filterUsesSlope() ? biquadStagesForSlope(slope) : 1;
            
            if (slope == FilterSlope::dB6)
            {
                // 1st order highpass
                const double tan_half_w0 = std::tan(juce::MathConstants<double>::pi * clampedF0 / sampleRate);
                a0 = 1.0 + tan_half_w0;
                b0 = 1.0 / a0;
                b1 = -1.0 / a0;
                b2 = 0.0;
                a1 = (tan_half_w0 - 1.0) / a0;
                a2 = 0.0;
                a0 = 1.0; // Already normalized above
            }
            else
            {
                // 2nd order highpass (cascaded for steeper slopes)
                b0 = (1.0 + cos_w0) / 2.0;
                b1 = -(1.0 + cos_w0);
                b2 = (1.0 + cos_w0) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cos_w0;
                a2 = 1.0 - alpha;
            }
            break;
        }
        case FilterType::HighCut:
        {
            numStages = params.filterUsesSlope() ? biquadStagesForSlope(slope) : 1;
            
            if (slope == FilterSlope::dB6)
            {
                // 1st order lowpass
                const double tan_half_w0 = std::tan(juce::MathConstants<double>::pi * clampedF0 / sampleRate);
                a0 = 1.0 + tan_half_w0;
                b0 = tan_half_w0 / a0;
                b1 = tan_half_w0 / a0;
                b2 = 0.0;
                a1 = (tan_half_w0 - 1.0) / a0;
                a2 = 0.0;
                a0 = 1.0; // Already normalized above
            }
            else
            {
                // 2nd order lowpass
                b0 = (1.0 - cos_w0) / 2.0;
                b1 = 1.0 - cos_w0;
                b2 = (1.0 - cos_w0) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cos_w0;
                a2 = 1.0 - alpha;
            }
            break;
        }
        case FilterType::LowShelf:
        {
            const double two_sqrtA_alpha = 2.0 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * cos_w0 + two_sqrtA_alpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0);
            b2 = A * ((A + 1.0) - (A - 1.0) * cos_w0 - two_sqrtA_alpha);
            a0 = (A + 1.0) + (A - 1.0) * cos_w0 + two_sqrtA_alpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_w0);
            a2 = (A + 1.0) + (A - 1.0) * cos_w0 - two_sqrtA_alpha;
            break;
        }
        case FilterType::HighShelf:
        {
            const double two_sqrtA_alpha = 2.0 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * cos_w0 + two_sqrtA_alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0);
            b2 = A * ((A + 1.0) + (A - 1.0) * cos_w0 - two_sqrtA_alpha);
            a0 = (A + 1.0) - (A - 1.0) * cos_w0 + two_sqrtA_alpha;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_w0);
            a2 = (A + 1.0) - (A - 1.0) * cos_w0 - two_sqrtA_alpha;
            break;
        }
        case FilterType::Notch:
        {
            b0 = 1.0;
            b1 = -2.0 * cos_w0;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_w0;
            a2 = 1.0 - alpha;
            break;
        }
        case FilterType::BandPass:
        {
            // Constant 0 dB peak gain bandpass
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos_w0;
            a2 = 1.0 - alpha;
            break;
        }
        case FilterType::TiltShelf:
        {
            // 1st-order tilt shelf implementation
            const double tan_half_w0 = std::tan(juce::MathConstants<double>::pi * clampedF0 / sampleRate);
            b0 = tan_half_w0 + A;
            b1 = tan_half_w0 - A;
            b2 = 0.0;
            a0 = tan_half_w0 * A + 1.0;
            a1 = tan_half_w0 * A - 1.0;
            a2 = 0.0;
            break;
        }
    }

    // Normalize coefficients by a0
    if (a0 != 0.0 && a0 != 1.0)
    {
        const double invA0 = 1.0 / a0;
        b0 *= invA0;
        b1 *= invA0;
        b2 *= invA0;
        a1 *= invA0;
        a2 *= invA0;
    }

    // Ensure we don't exceed the max number of biquads in our array
    numStages = std::min(numStages, static_cast<int>(biquads.size()));

    // Apply coefficients to all active biquad stages
    for (int i = 0; i < numStages; ++i)
    {
        biquads[i].setCoefficients(b0, b1, b2, a1, a2);
    }
}

} // namespace ProEQ
