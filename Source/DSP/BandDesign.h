#pragma once

#include "Utilities/Constants.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace AxisEQ
{

/// Normalised biquad coefficients (a0 == 1).
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    /// Complex response at z^-1 = @p zInv.
    std::complex<double> response(std::complex<double> zInv) const noexcept
    {
        return (b0 + zInv * (b1 + zInv * b2)) / (1.0 + zInv * (a1 + zInv * a2));
    }
};

/// 96 dB/oct is a 16th-order cut, i.e. 8 cascaded biquads.
inline constexpr int MaxStages = 8;

/// The complete filter for one band: up to MaxStages cascaded biquads.
/// The audio engine and the EQ curve both use this, so what you see is what you hear.
struct BandDesign
{
    std::array<BiquadCoeffs, MaxStages> stages;
    int numStages = 1;

    /// Linear magnitude of the whole cascade at @p frequency.
    double magnitude(double frequency, double sampleRate) const noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const std::complex<double> zInv = std::polar(1.0, -w);
        std::complex<double> h(1.0, 0.0);
        for (int i = 0; i < numStages; ++i)
            h *= stages[static_cast<size_t>(i)].response(zInv);
        return std::abs(h);
    }
};

namespace detail
{
    inline BiquadCoeffs normalised(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        const double inv = 1.0 / a0;
        return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
    }

    /// Low or high cut of the slope's Butterworth order. Each biquad gets its own Butterworth
    /// Q (cascading identical sections would sag by 3 dB per section at the cutoff); the band's
    /// Q scales the most resonant section, so 0.707 is a plain Butterworth and 12 dB/oct cuts
    /// respond exactly as a single RBJ section with that Q.
    inline void designCut(BandDesign& d, bool highPass, FilterSlope slope,
                          double f0, double q, double sampleRate) noexcept
    {
        const int order = filterOrderForSlope(slope);
        const double pi = juce::MathConstants<double>::pi;

        if (order == 1)
        {
            const double t = std::tan(pi * f0 / sampleRate);
            d.numStages = 1;
            d.stages[0] = highPass ? normalised(1.0, -1.0, 0.0, 1.0 + t, t - 1.0, 0.0)
                                   : normalised(t, t, 0.0, 1.0 + t, t - 1.0, 0.0);
            return;
        }

        const int sections = std::min(order / 2, MaxStages);
        const double w0 = juce::MathConstants<double>::twoPi * f0 / sampleRate;
        const double cosW0 = std::cos(w0), sinW0 = std::sin(w0);
        d.numStages = sections;

        for (int k = 0; k < sections; ++k)
        {
            double sectionQ = 1.0 / (2.0 * std::cos(pi * (2.0 * k + 1.0) / (2.0 * order)));
            if (k == sections - 1)
                sectionQ *= q * juce::MathConstants<double>::sqrt2;   // relative to Butterworth's 1/sqrt(2)

            const double alpha = sinW0 / (2.0 * sectionQ);
            d.stages[static_cast<size_t>(k)] = highPass
                ? normalised((1.0 + cosW0) / 2.0, -(1.0 + cosW0), (1.0 + cosW0) / 2.0, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha)
                : normalised((1.0 - cosW0) / 2.0, 1.0 - cosW0, (1.0 - cosW0) / 2.0, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
        }
    }
}

/// Designs the filter for one band. @p gainDb is ignored by types that have no gain.
inline BandDesign designBand(FilterType type, FilterSlope slope, double frequency, double q,
                             double gainDb, double sampleRate) noexcept
{
    BandDesign d;
    const double f0 = std::clamp(frequency, 10.0, sampleRate * 0.49);
    const double safeQ = std::max(q, 0.01);
    const double w0 = juce::MathConstants<double>::twoPi * f0 / sampleRate;
    const double cosW0 = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * safeQ);
    const double A = std::pow(10.0, gainDb / 40.0);
    auto& c = d.stages[0];

    switch (type)
    {
        case FilterType::Bell:
            c = detail::normalised(1.0 + alpha * A, -2.0 * cosW0, 1.0 - alpha * A,
                                   1.0 + alpha / A, -2.0 * cosW0, 1.0 - alpha / A);
            break;

        case FilterType::LowCut:
            detail::designCut(d, true, slope, f0, safeQ, sampleRate);
            break;

        case FilterType::HighCut:
            detail::designCut(d, false, slope, f0, safeQ, sampleRate);
            break;

        case FilterType::LowShelf:
        {
            const double k = 2.0 * std::sqrt(A) * alpha;
            c = detail::normalised(A * ((A + 1.0) - (A - 1.0) * cosW0 + k), 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0),
                                   A * ((A + 1.0) - (A - 1.0) * cosW0 - k), (A + 1.0) + (A - 1.0) * cosW0 + k,
                                   -2.0 * ((A - 1.0) + (A + 1.0) * cosW0), (A + 1.0) + (A - 1.0) * cosW0 - k);
            break;
        }

        case FilterType::HighShelf:
        {
            const double k = 2.0 * std::sqrt(A) * alpha;
            c = detail::normalised(A * ((A + 1.0) + (A - 1.0) * cosW0 + k), -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0),
                                   A * ((A + 1.0) + (A - 1.0) * cosW0 - k), (A + 1.0) - (A - 1.0) * cosW0 + k,
                                   2.0 * ((A - 1.0) - (A + 1.0) * cosW0), (A + 1.0) - (A - 1.0) * cosW0 - k);
            break;
        }

        case FilterType::Notch:
            c = detail::normalised(1.0, -2.0 * cosW0, 1.0, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
            break;

        case FilterType::BandPass:
            // Constant 0 dB peak gain.
            c = detail::normalised(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
            break;

        case FilterType::TiltShelf:
        {
            // First-order tilt: -gain/2 at DC, +gain/2 at Nyquist, pivoting around f0.
            const double t = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate);
            c = detail::normalised(t + A, t - A, 0.0, t * A + 1.0, t * A - 1.0, 0.0);
            break;
        }
    }

    return d;
}

/// True for the types whose gain the dynamic stage can move.
inline bool typeUsesGain(FilterType type) noexcept
{
    return type == FilterType::Bell || type == FilterType::LowShelf
        || type == FilterType::HighShelf || type == FilterType::TiltShelf;
}

} // namespace AxisEQ
