#pragma once

#include <JuceHeader.h>
#include "Utilities/Constants.h"
#include <atomic>

namespace ProEQ
{

/// Holds cached pointers to the raw atomic parameter values for one EQ band.
///
/// All pointers are obtained from APVTS::getRawParameterValue() during
/// construction and remain valid for the lifetime of the processor.
/// Getters use std::memory_order_relaxed for minimal overhead on the
/// audio thread — this is safe because parameter changes are single-writer
/// (the host / GUI) and we can tolerate a one-block delay in visibility.
struct FilterBandParameters
{
    // ── Raw atomic pointers (set once by bindToAPVTS, never re-assigned) ─

    std::atomic<float>* frequency    = nullptr;
    std::atomic<float>* gain         = nullptr;
    std::atomic<float>* q            = nullptr;
    std::atomic<float>* filterType   = nullptr;
    std::atomic<float>* slope        = nullptr;
    std::atomic<float>* bypassed     = nullptr;
    std::atomic<float>* routingMode  = nullptr;
    std::atomic<float>* active       = nullptr;
    std::atomic<float>* dynEnabled   = nullptr;
    std::atomic<float>* dynThreshold = nullptr;
    std::atomic<float>* dynRange     = nullptr;
    std::atomic<float>* dynAttack    = nullptr;
    std::atomic<float>* dynRelease   = nullptr;

    // ── Audio-thread-safe getters ────────────────────────────────────────

    float getFrequency() const noexcept
    {
        return frequency->load(std::memory_order_relaxed);
    }

    float getGain() const noexcept
    {
        return gain->load(std::memory_order_relaxed);
    }

    float getQ() const noexcept
    {
        return q->load(std::memory_order_relaxed);
    }

    FilterType getFilterType() const noexcept
    {
        return static_cast<FilterType>(
            static_cast<int>(filterType->load(std::memory_order_relaxed)));
    }

    FilterSlope getSlope() const noexcept
    {
        return static_cast<FilterSlope>(
            static_cast<int>(slope->load(std::memory_order_relaxed)));
    }

    bool isBypassed() const noexcept
    {
        return bypassed->load(std::memory_order_relaxed) >= 0.5f;
    }

    RoutingMode getRoutingMode() const noexcept
    {
        return static_cast<RoutingMode>(
            static_cast<int>(routingMode->load(std::memory_order_relaxed)));
    }

    bool isActive() const noexcept
    {
        return active->load(std::memory_order_relaxed) >= 0.5f;
    }

    bool isDynEnabled() const noexcept
    {
        return dynEnabled->load(std::memory_order_relaxed) >= 0.5f;
    }

    float getDynThreshold() const noexcept
    {
        return dynThreshold->load(std::memory_order_relaxed);
    }

    float getDynRange() const noexcept
    {
        return dynRange->load(std::memory_order_relaxed);
    }

    /// Attack time in milliseconds.
    float getDynAttackMs() const noexcept
    {
        return dynAttack->load(std::memory_order_relaxed);
    }

    /// Release time in milliseconds.
    float getDynReleaseMs() const noexcept
    {
        return dynRelease->load(std::memory_order_relaxed);
    }

    // ── Convenience queries ─────────────────────────────────────────────

    /// True if this band should produce audible output (active AND not bypassed).
    bool shouldProcess() const noexcept
    {
        return isActive() && !isBypassed();
    }

    /// True for filter types that use gain (bell, shelves, tilt).
    /// Cut, notch, and bandpass filters ignore the gain parameter.
    bool filterUsesGain() const noexcept
    {
        const auto t = getFilterType();
        return t == FilterType::Bell
            || t == FilterType::LowShelf
            || t == FilterType::HighShelf
            || t == FilterType::TiltShelf;
    }

    /// True for filter types that use the slope parameter (cuts only).
    bool filterUsesSlope() const noexcept
    {
        const auto t = getFilterType();
        return t == FilterType::LowCut || t == FilterType::HighCut;
    }

    // ── Binding (called once from the processor constructor) ────────────

    /// Wires all 13 parameter pointers for band @p bandIndex from the APVTS.
    /// Must be called on the message thread before audio processing begins.
    void bindToAPVTS(juce::AudioProcessorValueTreeState& apvts, int bandIndex)
    {
        frequency    = apvts.getRawParameterValue(paramID("freq",          bandIndex));
        gain         = apvts.getRawParameterValue(paramID("gain",          bandIndex));
        q            = apvts.getRawParameterValue(paramID("q",             bandIndex));
        filterType   = apvts.getRawParameterValue(paramID("type",          bandIndex));
        slope        = apvts.getRawParameterValue(paramID("slope",         bandIndex));
        bypassed     = apvts.getRawParameterValue(paramID("bypass",        bandIndex));
        routingMode  = apvts.getRawParameterValue(paramID("route",         bandIndex));
        active       = apvts.getRawParameterValue(paramID("active",        bandIndex));
        dynEnabled   = apvts.getRawParameterValue(paramID("dyn_enabled",   bandIndex));
        dynThreshold = apvts.getRawParameterValue(paramID("dyn_threshold", bandIndex));
        dynRange     = apvts.getRawParameterValue(paramID("dyn_range",     bandIndex));
        dynAttack    = apvts.getRawParameterValue(paramID("dyn_attack",    bandIndex));
        dynRelease   = apvts.getRawParameterValue(paramID("dyn_release",   bandIndex));

        jassert(frequency    != nullptr);
        jassert(gain         != nullptr);
        jassert(q            != nullptr);
        jassert(filterType   != nullptr);
        jassert(slope        != nullptr);
        jassert(bypassed     != nullptr);
        jassert(routingMode  != nullptr);
        jassert(active       != nullptr);
        jassert(dynEnabled   != nullptr);
        jassert(dynThreshold != nullptr);
        jassert(dynRange     != nullptr);
        jassert(dynAttack    != nullptr);
        jassert(dynRelease   != nullptr);
    }
};

} // namespace ProEQ
