#include "BandProcessor.h"
#include <algorithm>
#include <cmath>

namespace AxisEQ
{

//──────────────────────────────────────────────────────────────────────────────
// Cascade
//──────────────────────────────────────────────────────────────────────────────
void BandProcessor::Cascade::clear() noexcept
{
    for (int s = 0; s < MaxStages; ++s)
        for (int ch = 0; ch < 2; ++ch)
            z1[s][ch] = z2[s][ch] = 0.0;
}

float BandProcessor::Cascade::process(int channel, float input) noexcept
{
    double x = static_cast<double>(input);
    for (int s = 0; s < design.numStages; ++s)
    {
        const auto& c = design.stages[static_cast<size_t>(s)];
        const double y = c.b0 * x + z1[s][channel];
        z1[s][channel] = c.b1 * x - c.a1 * y + z2[s][channel];
        z2[s][channel] = c.b2 * x - c.a2 * y;
        x = y;
    }
    return static_cast<float>(x);
}

//──────────────────────────────────────────────────────────────────────────────
// Setup
//──────────────────────────────────────────────────────────────────────────────
void BandProcessor::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    frequency.reset(sampleRate, 0.05);
    q.reset(sampleRate, 0.05);
    gainDb.reset(sampleRate, 0.05);
    dynamicAmount.reset(sampleRate, 0.02);
    mix.reset(sampleRate, 0.01);
    reset();
}

void BandProcessor::reset() noexcept
{
    filter.clear();
    detector.clear();
    envelope = 0.0;
    dynamicGainDb = 0.0;
    mix.setCurrentAndTargetValue(0.0f);
    pendingStructure = false;
    needsDesign = true;
    dynamicGainForGui.store(0.0f, std::memory_order_relaxed);
}

/// Switches to a new type, slope or routing. Only called while the band is silent
/// (mix at 0), so clearing the filter state cannot be heard.
void BandProcessor::adopt(const Structure& next) noexcept
{
    structure = next;
    filter.clear();
    detector.clear();
    envelope = 0.0;
    pendingStructure = false;
    needsDesign = true;
}

void BandProcessor::redesign(double f, double bandQ, double staticGainDb) noexcept
{
    const double totalGainDb = staticGainDb + dynamicGainDb * dynamicAmount.getCurrentValue();
    filter.design = designBand(structure.type, structure.slope, f, bandQ, totalGainDb, sampleRate);

    // Detector matched to the band, so dynamics react only to the band's own range.
    hasDetector = true;
    switch (structure.type)
    {
        case FilterType::Bell:      detector.design = designBand(FilterType::BandPass, FilterSlope::dB12, f, bandQ, 0.0, sampleRate); break;
        case FilterType::LowShelf:  detector.design = designBand(FilterType::HighCut, FilterSlope::dB12, f, 0.7071, 0.0, sampleRate); break;
        case FilterType::HighShelf: detector.design = designBand(FilterType::LowCut, FilterSlope::dB12, f, 0.7071, 0.0, sampleRate); break;
        case FilterType::TiltShelf:                     // reacts to the full band
        case FilterType::LowCut:
        case FilterType::HighCut:
        case FilterType::Notch:
        case FilterType::BandPass:  hasDetector = false; break;   // no dynamics for these types
    }
}

/// Soft-knee gain computer. A negative range cuts when the band gets loud, a positive
/// range boosts; either way the change is limited to |range| dB.
double BandProcessor::dynamicGainFor(double levelDb, double thresholdDb, double rangeDb) const noexcept
{
    const double over = levelDb - thresholdDb;
    double amount = 0.0;
    if (2.0 * over >= KneeDb)
        amount = over;
    else if (2.0 * over > -KneeDb)
        amount = (over + KneeDb / 2.0) * (over + KneeDb / 2.0) / (2.0 * KneeDb);

    const double change = amount * (1.0 - 1.0 / Ratio);
    return rangeDb < 0.0 ? std::max(-change, rangeDb) : std::min(change, rangeDb);
}

//──────────────────────────────────────────────────────────────────────────────
// Processing
//──────────────────────────────────────────────────────────────────────────────
void BandProcessor::process(float* left, float* right, int numSamples,
                            const float* scLeft, const float* scRight,
                            const FilterBandParameters& params) noexcept
{
    const Structure wanted { params.getFilterType(), params.getSlope(), params.getRoutingMode() };
    const bool wantActive = params.shouldProcess();

    // Structural changes fade the band out, switch, then fade back in.
    if (wanted != structure)
    {
        if (mix.getCurrentValue() <= 0.0f && ! mix.isSmoothing())
            adopt(wanted);
        else
        {
            pendingStructure = true;
            nextStructure = wanted;
            mix.setTargetValue(0.0f);
        }
    }
    else
    {
        pendingStructure = false;   // changed back before the fade-out finished
    }
    if (! pendingStructure)
        mix.setTargetValue(wantActive ? 1.0f : 0.0f);

    // Fully out and staying out: nothing to do.
    if (! pendingStructure && ! mix.isSmoothing() && mix.getCurrentValue() <= 0.0f)
    {
        dynamicGainDb = 0.0;
        dynamicGainForGui.store(0.0f, std::memory_order_relaxed);
        needsDesign = true;
        return;
    }

    if (needsDesign)
    {
        frequency.setCurrentAndTargetValue(params.getFrequency());
        q.setCurrentAndTargetValue(params.getQ());
        gainDb.setCurrentAndTargetValue(params.getGain());
    }
    frequency.setTargetValue(std::clamp(static_cast<double>(params.getFrequency()), 10.0, sampleRate * 0.49));
    q.setTargetValue(std::max(0.01, static_cast<double>(params.getQ())));
    gainDb.setTargetValue(params.getGain());

    const bool dynamic = params.isDynEnabled() && typeUsesGain(structure.type);
    dynamicAmount.setTargetValue(dynamic ? 1.0 : 0.0);

    const double thresholdDb = params.getDynThreshold();
    const double rangeDb = params.getDynRange();
    const double attackCoef = std::exp(-1.0 / (std::max(0.01, static_cast<double>(params.getDynAttackMs())) * 0.001 * sampleRate));
    const double releaseCoef = std::exp(-1.0 / (std::max(0.01, static_cast<double>(params.getDynReleaseMs())) * 0.001 * sampleRate));
    const bool external = params.isDynExtScEnabled() && scLeft != nullptr;
    const bool stereo = right != nullptr;

    for (int start = 0; start < numSamples; start += UpdateInterval)
    {
        const int end = std::min(numSamples, start + UpdateInterval);
        const int len = end - start;

        const bool moving = frequency.isSmoothing() || q.isSmoothing() || gainDb.isSmoothing() || dynamicAmount.isSmoothing();
        const double f = frequency.skip(len), bandQ = q.skip(len), g = gainDb.skip(len);
        dynamicAmount.skip(len);
        if (needsDesign || moving || dynamic)
        {
            redesign(f, bandQ, g);
            needsDesign = false;
        }

        for (int n = start; n < end; ++n)
        {
            const float wet = mix.getNextValue();

            // The signal this band works on, by routing.
            const float l = left[n], r = stereo ? right[n] : left[n];
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r);
            float in[2] { l, r };
            int channels = stereo ? 2 : 1;
            if (stereo && structure.route != RoutingMode::Stereo)
            {
                in[0] = structure.route == RoutingMode::MidOnly ? mid : side;
                channels = 1;
            }

            if (dynamic)
            {
                float sc[2] { in[0], in[1] };
                if (external)
                {
                    const float sl = scLeft[n], sr = scRight != nullptr ? scRight[n] : scLeft[n];
                    sc[0] = structure.route == RoutingMode::SideOnly ? 0.5f * (sl - sr)
                          : structure.route == RoutingMode::MidOnly  ? 0.5f * (sl + sr) : sl;
                    sc[1] = sr;
                }

                // Linked: one envelope from the louder channel drives both.
                double level = 0.0;
                for (int ch = 0; ch < channels; ++ch)
                    level = std::max(level, std::abs(static_cast<double>(hasDetector ? detector.process(ch, sc[ch]) : sc[ch])));

                const double coef = level > envelope ? attackCoef : releaseCoef;
                envelope = level + coef * (envelope - level);
                dynamicGainDb = dynamicGainFor(20.0 * std::log10(std::max(envelope, 1.0e-9)), thresholdDb, rangeDb);
            }

            float out[2];
            for (int ch = 0; ch < channels; ++ch)
                out[ch] = in[ch] + wet * (filter.process(ch, in[ch]) - in[ch]);

            if (! stereo)
                left[n] = out[0];
            else if (structure.route == RoutingMode::Stereo)
            {
                left[n] = out[0];
                right[n] = out[1];
            }
            else
            {
                const float m = structure.route == RoutingMode::MidOnly ? out[0] : mid;
                const float s = structure.route == RoutingMode::SideOnly ? out[0] : side;
                left[n] = m + s;
                right[n] = m - s;
            }

            // A pending structural change takes effect once the band has faded out.
            if (pendingStructure && wet <= 0.0f && ! mix.isSmoothing())
            {
                adopt(nextStructure);
                mix.setTargetValue(wantActive ? 1.0f : 0.0f);
                redesign(frequency.getCurrentValue(), q.getCurrentValue(), gainDb.getCurrentValue());
                needsDesign = false;
            }
        }
    }

    dynamicGainForGui.store(static_cast<float>(dynamicGainDb * dynamicAmount.getCurrentValue()), std::memory_order_relaxed);
}

} // namespace AxisEQ
