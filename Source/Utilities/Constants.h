#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>

namespace ProEQ
{

// ─────────────────────────────────────────────────────────────────────────────
// Global sizing
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr int MaxBands = 24;

// ─────────────────────────────────────────────────────────────────────────────
// Frequency (Hz)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinFrequency     = 20.0f;
inline constexpr float MaxFrequency     = 20000.0f;
inline constexpr float DefaultFrequency = 1000.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Gain (dB)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinGain     = -30.0f;
inline constexpr float MaxGain     =  30.0f;
inline constexpr float DefaultGain =   0.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Q factor
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinQ     = 0.025f;
inline constexpr float MaxQ     = 40.0f;
inline constexpr float DefaultQ = 0.707f;

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic EQ – Threshold (dB)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinThreshold     = -60.0f;
inline constexpr float MaxThreshold     =   0.0f;
inline constexpr float DefaultThreshold = -20.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic EQ – Range (dB)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinDynRange     = -30.0f;
inline constexpr float MaxDynRange     =  30.0f;
inline constexpr float DefaultDynRange =   0.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic EQ – Attack (ms)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinAttack     =   0.1f;
inline constexpr float MaxAttack     = 500.0f;
inline constexpr float DefaultAttack =  10.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Dynamic EQ – Release (ms)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr float MinRelease     =    1.0f;
inline constexpr float MaxRelease     = 5000.0f;
inline constexpr float DefaultRelease =  100.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum analyzer
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr int   FFTOrder        = 12;               // 2^12 = 4096 points
inline constexpr int   FFTSize         = 1 << FFTOrder;    // 4096
inline constexpr float AnalyzerDecayRate = 0.85f;          // per-frame decay multiplier

// ─────────────────────────────────────────────────────────────────────────────
// Enumerations
// ─────────────────────────────────────────────────────────────────────────────

enum class FilterType : int
{
    Bell       = 0,
    LowCut     = 1,
    HighCut    = 2,
    LowShelf   = 3,
    HighShelf  = 4,
    Notch      = 5,
    BandPass   = 6,
    TiltShelf  = 7
};

inline constexpr int NumFilterTypes = 8;

enum class FilterSlope : int
{
    dB6  = 0,    //  6 dB/oct  →  1st-order (single 1-pole biquad)
    dB12 = 1,    // 12 dB/oct  →  2nd-order (single biquad)
    dB24 = 2,    // 24 dB/oct  →  4th-order (2 cascaded biquads)
    dB48 = 3,    // 48 dB/oct  →  8th-order (4 cascaded biquads)
    dB96 = 4     // 96 dB/oct  → 16th-order (8 cascaded biquads)
};

inline constexpr int NumFilterSlopes = 5;

/// Returns the number of cascaded biquad sections needed for the given slope.
inline constexpr int biquadStagesForSlope(FilterSlope slope) noexcept
{
    constexpr int table[] = { 1, 1, 2, 4, 8 };
    const auto idx = static_cast<int>(slope);
    return (idx >= 0 && idx < NumFilterSlopes) ? table[idx] : 1;
}

/// Returns the filter order for the given slope (poles per channel).
inline constexpr int filterOrderForSlope(FilterSlope slope) noexcept
{
    constexpr int table[] = { 1, 2, 4, 8, 16 };
    const auto idx = static_cast<int>(slope);
    return (idx >= 0 && idx < NumFilterSlopes) ? table[idx] : 2;
}

enum class RoutingMode : int
{
    Stereo   = 0,
    MidOnly  = 1,
    SideOnly = 2
};

inline constexpr int NumRoutingModes = 3;

// ─────────────────────────────────────────────────────────────────────────────
// String arrays for parameter choices (used by AudioParameterChoice)
// ─────────────────────────────────────────────────────────────────────────────

inline const juce::StringArray FilterTypeNames {
    "Bell", "Low Cut", "High Cut", "Low Shelf",
    "High Shelf", "Notch", "Band Pass", "Tilt Shelf"
};

inline const juce::StringArray FilterSlopeNames {
    "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct", "96 dB/oct"
};

inline const juce::StringArray RoutingModeNames {
    "Stereo", "Mid", "Side"
};

// ─────────────────────────────────────────────────────────────────────────────
// Parameter-ID generation helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Builds a unique APVTS parameter ID for a given band index and suffix.
/// Example: paramID("freq", 3) → "band3_freq"
inline juce::String paramID(const juce::String& paramName, int bandIndex)
{
    return "band" + juce::String(bandIndex) + "_" + paramName;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-band default values
// ─────────────────────────────────────────────────────────────────────────────

/// Logarithmically spread default centre frequencies for all 24 bands.
inline constexpr std::array<float, MaxBands> DefaultBandFrequencies = {{
    30.0f,    50.0f,    80.0f,    120.0f,   200.0f,   350.0f,   500.0f,   800.0f,
    1200.0f,  2000.0f,  3500.0f,  5000.0f,  7000.0f,  10000.0f, 14000.0f, 18000.0f,
    50.0f,    100.0f,   250.0f,   600.0f,   1500.0f,  3000.0f,  6000.0f,  12000.0f
}};

/// Default filter types for every band (first/last are cuts, rest are bells).
inline constexpr std::array<FilterType, MaxBands> DefaultBandTypes = {{
    FilterType::LowCut,  FilterType::Bell,    FilterType::Bell,    FilterType::Bell,
    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,
    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,
    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,    FilterType::HighCut,
    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,
    FilterType::Bell,    FilterType::Bell,    FilterType::Bell,    FilterType::Bell
}};

// ─────────────────────────────────────────────────────────────────────────────
// Node colours (for GUI – one distinct colour per band for easy identification)
// ─────────────────────────────────────────────────────────────────────────────

inline const std::array<juce::Colour, MaxBands> BandColours = {{
    juce::Colour(0xFFe74c3c), juce::Colour(0xFFe67e22), juce::Colour(0xFFf1c40f),
    juce::Colour(0xFF2ecc71), juce::Colour(0xFF1abc9c), juce::Colour(0xFF3498db),
    juce::Colour(0xFF9b59b6), juce::Colour(0xFFe91e63), juce::Colour(0xFFff5722),
    juce::Colour(0xFFff9800), juce::Colour(0xFFcddc39), juce::Colour(0xFF4caf50),
    juce::Colour(0xFF009688), juce::Colour(0xFF00bcd4), juce::Colour(0xFF2196f3),
    juce::Colour(0xFF673ab7), juce::Colour(0xFFf44336), juce::Colour(0xFFff6f00),
    juce::Colour(0xFFaeea00), juce::Colour(0xFF00e676), juce::Colour(0xFF18ffff),
    juce::Colour(0xFF448aff), juce::Colour(0xFFd500f9), juce::Colour(0xFFff1744)
}};

} // namespace ProEQ
