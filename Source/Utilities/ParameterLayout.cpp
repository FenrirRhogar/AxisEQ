#include "ParameterLayout.h"
#include "Constants.h"

namespace ProEQ
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < MaxBands; ++i)
    {
        const auto bandStr  = juce::String(i);
        const auto prefix   = "band" + bandStr + "_";
        const auto bandName = "Band " + bandStr + " ";

        // ─── Frequency ──────────────────────────────────────────────────
        // Logarithmic mapping: setSkewForCentre(1000) places 1 kHz at the
        // midpoint of the slider, giving perceptually uniform resolution.
        auto freqRange = juce::NormalisableRange<float>(MinFrequency, MaxFrequency);
        freqRange.setSkewForCentre(1000.0f);

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "freq", 1),
            bandName + "Frequency",
            freqRange,
            DefaultBandFrequencies[static_cast<size_t>(i)],
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    if (value >= 1000.0f)
                        return juce::String(value / 1000.0f, 2) + " kHz";
                    return juce::String(static_cast<int>(value)) + " Hz";
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    if (text.containsIgnoreCase("k"))
                        return text.getFloatValue() * 1000.0f;
                    return text.getFloatValue();
                })
        ));

        // ─── Gain ───────────────────────────────────────────────────────
        // Linear mapping, 0.1 dB step size for smooth automation.
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "gain", 1),
            bandName + "Gain",
            juce::NormalisableRange<float>(MinGain, MaxGain, 0.1f),
            DefaultGain,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    return juce::String(value, 1) + " dB";
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    return text.getFloatValue();
                })
        ));

        // ─── Q Factor ───────────────────────────────────────────────────
        // Logarithmic mapping: setSkewForCentre(1.0) gives fine control
        // near the perceptually important narrow-bandwidth region.
        auto qRange = juce::NormalisableRange<float>(MinQ, MaxQ);
        qRange.setSkewForCentre(1.0f);

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "q", 1),
            bandName + "Q",
            qRange,
            DefaultQ,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    return juce::String(value, 3);
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    return text.getFloatValue();
                })
        ));

        // ─── Filter Type (choice) ──────────────────────────────────────
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "type", 1),
            bandName + "Type",
            FilterTypeNames,
            static_cast<int>(DefaultBandTypes[static_cast<size_t>(i)])
        ));

        // ─── Slope (choice) ────────────────────────────────────────────
        // Default to 12 dB/oct (index 1) — the most common EQ slope.
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "slope", 1),
            bandName + "Slope",
            FilterSlopeNames,
            1
        ));

        // ─── Bypass (bool) ─────────────────────────────────────────────
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "bypass", 1),
            bandName + "Bypass",
            false
        ));

        // ─── Routing mode (choice) ─────────────────────────────────────
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "route", 1),
            bandName + "Route",
            RoutingModeNames,
            static_cast<int>(RoutingMode::Stereo)
        ));

        // ─── Active (bool) ─────────────────────────────────────────────
        // All bands default to inactive; the user activates them on demand.
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "active", 1),
            bandName + "Active",
            false
        ));

        // ─── Dynamic EQ: Enabled (bool) ────────────────────────────────
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "dyn_enabled", 1),
            bandName + "Dynamic Enabled",
            false
        ));

        // ─── Dynamic EQ: Threshold ─────────────────────────────────────
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "dyn_threshold", 1),
            bandName + "Dynamic Threshold",
            juce::NormalisableRange<float>(MinThreshold, MaxThreshold, 0.1f),
            DefaultThreshold,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    return juce::String(value, 1) + " dB";
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    return text.getFloatValue();
                })
        ));

        // ─── Dynamic EQ: Range ─────────────────────────────────────────
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "dyn_range", 1),
            bandName + "Dynamic Range",
            juce::NormalisableRange<float>(MinDynRange, MaxDynRange, 0.1f),
            DefaultDynRange,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    return juce::String(value, 1) + " dB";
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    return text.getFloatValue();
                })
        ));

        // ─── Dynamic EQ: Attack ────────────────────────────────────────
        // Logarithmic with 10 ms as perceptual centre.
        auto attackRange = juce::NormalisableRange<float>(MinAttack, MaxAttack);
        attackRange.setSkewForCentre(10.0f);

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "dyn_attack", 1),
            bandName + "Dynamic Attack",
            attackRange,
            DefaultAttack,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    return juce::String(value, 1) + " ms";
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    return text.getFloatValue();
                })
        ));

        // ─── Dynamic EQ: Release ───────────────────────────────────────
        // Logarithmic with 100 ms as perceptual centre.
        auto releaseRange = juce::NormalisableRange<float>(MinRelease, MaxRelease);
        releaseRange.setSkewForCentre(100.0f);

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "dyn_release", 1),
            bandName + "Dynamic Release",
            releaseRange,
            DefaultRelease,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int /*maxLen*/)
                {
                    if (value >= 1000.0f)
                        return juce::String(value / 1000.0f, 2) + " s";
                    return juce::String(value, 1) + " ms";
                })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    if (text.containsIgnoreCase("s") && !text.containsIgnoreCase("ms"))
                        return text.getFloatValue() * 1000.0f;
                    return text.getFloatValue();
                })
        ));
    }

    return layout;
}

} // namespace ProEQ
