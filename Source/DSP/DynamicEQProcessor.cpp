#include "DynamicEQProcessor.h"
#include <cmath>
#include <algorithm>

namespace ProEQ
{

DynamicEQProcessor::DynamicEQProcessor()
{
    updateEnvelopeCoefficients();
    updateFilterBase();
}

void DynamicEQProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    channelStates.resize(spec.numChannels);
    
    reset();
    updateEnvelopeCoefficients();
    updateFilterBase();
}

void DynamicEQProcessor::reset()
{
    linkedEnvelopeState = 0.0;
    for (auto& state : channelStates)
    {
        state.envelopeState = 0.0;
        state.z1 = 0.0;
        state.z2 = 0.0;
    }
}

void DynamicEQProcessor::setParameters(const DynamicEQBandParameters& params)
{
    parameters = params;
    updateEnvelopeCoefficients();
    updateFilterBase();
}

void DynamicEQProcessor::updateEnvelopeCoefficients()
{
    if (sampleRate <= 0.0) return;

    // Attack and release times are defined as the time to reach ~63% (1 - 1/e) of the target
    double attackSec = std::max(parameters.attackMs / 1000.0, 0.0001);
    double releaseSec = std::max(parameters.releaseMs / 1000.0, 0.0001);

    attackCoef = std::exp(-1.0 / (attackSec * sampleRate));
    releaseCoef = std::exp(-1.0 / (releaseSec * sampleRate));
}

void DynamicEQProcessor::updateFilterBase()
{
    if (sampleRate <= 0.0) return;

    // Constrain frequency to stable bounds (Nyquist limit)
    double freq = std::clamp(parameters.frequency, 10.0, sampleRate * 0.49);
    double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;

    cosW0 = std::cos(w0);
    double sinW0 = std::sin(w0);
    
    double safeQ = std::max(0.01, parameters.q);
    alpha = sinW0 / (2.0 * safeQ);
}

void DynamicEQProcessor::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const auto numSamples = block.getNumSamples();
    const auto numChannels = block.getNumChannels();

    if (numChannels == 0 || sampleRate <= 0.0) return;

    if (parameters.linked && numChannels > 1)
    {
        // ln(10) / 40 for fast dB to linear amplitude conversion
        constexpr double ln10_over_40 = 0.05756462732485114;

        for (size_t i = 0; i < numSamples; ++i)
        {
            // 1. Calculate sidechain level across all channels
            double sidechainInput = 0.0;
            if (parameters.useRms)
            {
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    double sample = static_cast<double>(block.getChannelPointer(ch)[i]);
                    sidechainInput += sample * sample;
                }
                sidechainInput /= static_cast<double>(numChannels);
            }
            else
            {
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    double sample = std::abs(static_cast<double>(block.getChannelPointer(ch)[i]));
                    sidechainInput = std::max(sidechainInput, sample);
                }
            }

            // 2. Update linked envelope
            if (sidechainInput > linkedEnvelopeState)
                linkedEnvelopeState = sidechainInput + attackCoef * (linkedEnvelopeState - sidechainInput);
            else
                linkedEnvelopeState = sidechainInput + releaseCoef * (linkedEnvelopeState - sidechainInput);

            // 3. Compute dynamic gain
            double currentLevel = parameters.useRms ? std::sqrt(std::max(linkedEnvelopeState, 1e-12)) 
                                                    : std::max(linkedEnvelopeState, 1e-12);
            
            double envDb = 20.0 * std::log10(currentLevel);
            double overshootDb = envDb - parameters.thresholdDb;
            double dynGainDb = 0.0;

            if (overshootDb > 0.0)
            {
                double ratioInv = (parameters.ratio > 0.0) ? (1.0 / parameters.ratio) : 0.0;
                dynGainDb = overshootDb * (ratioInv - 1.0);

                if (parameters.rangeDb < 0.0)
                    dynGainDb = std::clamp(dynGainDb, parameters.rangeDb, 0.0);
                else
                    dynGainDb = std::clamp(dynGainDb, 0.0, parameters.rangeDb);
            }

            double totalGainDb = parameters.staticGainDb + dynGainDb;

            // 4. Update filter coefficients for the current sample
            double A = std::exp(totalGainDb * ln10_over_40);
            double a0_inv = 1.0 / (1.0 + alpha / A);
            
            // RBJ Peaking EQ Formulas
            double b0 = (1.0 + alpha * A) * a0_inv;
            double b1 = -2.0 * cosW0 * a0_inv;
            double b2 = (1.0 - alpha * A) * a0_inv;
            double a1 = b1; // For peaking EQ, a1 is mathematically identical to b1 before normalization
            double a2 = (1.0 - alpha / A) * a0_inv;

            // 5. Apply filter to all channels (Transposed Direct Form II)
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& chState = channelStates[ch];
                double inSample = static_cast<double>(block.getChannelPointer(ch)[i]);
                
                double y = b0 * inSample + chState.z1;
                chState.z1 = b1 * inSample - a1 * y + chState.z2;
                chState.z2 = b2 * inSample - a2 * y;

                block.getChannelPointer(ch)[i] = static_cast<float>(y);
            }
        }
    }
    else
    {
        // Unlinked multi-mono processing
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = block.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                channelData[i] = processSample(static_cast<int>(ch), channelData[i]);
            }
        }
    }
}

float DynamicEQProcessor::processSample(int channel, float input)
{
    if (channel < 0 || channel >= static_cast<int>(channelStates.size()))
        return input;

    auto& state = channelStates[static_cast<size_t>(channel)];
    double inSample = static_cast<double>(input);

    // 1. Calculate sidechain level
    double sidechainInput = parameters.useRms ? (inSample * inSample) : std::abs(inSample);

    // 2. Update envelope
    if (sidechainInput > state.envelopeState)
        state.envelopeState = sidechainInput + attackCoef * (state.envelopeState - sidechainInput);
    else
        state.envelopeState = sidechainInput + releaseCoef * (state.envelopeState - sidechainInput);

    // 3. Compute dynamic gain
    double currentLevel = parameters.useRms ? std::sqrt(std::max(state.envelopeState, 1e-12)) 
                                            : std::max(state.envelopeState, 1e-12);
    
    double envDb = 20.0 * std::log10(currentLevel);
    double overshootDb = envDb - parameters.thresholdDb;
    double dynGainDb = 0.0;

    if (overshootDb > 0.0)
    {
        double ratioInv = (parameters.ratio > 0.0) ? (1.0 / parameters.ratio) : 0.0;
        dynGainDb = overshootDb * (ratioInv - 1.0);

        if (parameters.rangeDb < 0.0)
            dynGainDb = std::clamp(dynGainDb, parameters.rangeDb, 0.0);
        else
            dynGainDb = std::clamp(dynGainDb, 0.0, parameters.rangeDb);
    }

    double totalGainDb = parameters.staticGainDb + dynGainDb;

    // 4. Update filter coefficients
    constexpr double ln10_over_40 = 0.05756462732485114;
    double A = std::exp(totalGainDb * ln10_over_40);
    
    double a0_inv = 1.0 / (1.0 + alpha / A);
    double b0 = (1.0 + alpha * A) * a0_inv;
    double b1 = -2.0 * cosW0 * a0_inv;
    double b2 = (1.0 - alpha * A) * a0_inv;
    double a1 = b1;
    double a2 = (1.0 - alpha / A) * a0_inv;

    // 5. Apply filter (Transposed Direct Form II)
    double y = b0 * inSample + state.z1;
    state.z1 = b1 * inSample - a1 * y + state.z2;
    state.z2 = b2 * inSample - a2 * y;

    return static_cast<float>(y);
}

} // namespace ProEQ
