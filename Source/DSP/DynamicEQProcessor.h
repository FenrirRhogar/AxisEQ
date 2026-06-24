#pragma once

#include <JuceHeader.h>
#include <vector>

namespace ProEQ
{

struct DynamicEQBandParameters
{
    double frequency = 1000.0;
    double q = 0.707;
    double staticGainDb = 0.0;

    double thresholdDb = -20.0;
    double ratio = 2.0;       // > 1 for compression, < 1 for upward expansion
    double rangeDb = -6.0;    // Maximum dynamic gain applied (negative for cut, positive for boost)

    double attackMs = 15.0;
    double releaseMs = 100.0;

    bool useRms = false;      // false = Peak, true = RMS
    bool linked = true;       // true = Stereo linked, false = Multi-mono unlinked
};

class DynamicEQProcessor
{
public:
    DynamicEQProcessor();
    ~DynamicEQProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters(const DynamicEQBandParameters& params);

    // Processes a block. Uses linked or unlinked processing based on parameters.
    void process(const juce::dsp::ProcessContextReplacing<float>& context);
    
    // Processes a single sample for a specific channel (unlinked).
    float processSample(int channel, float input);

private:
    void updateEnvelopeCoefficients();
    void updateFilterBase();

    DynamicEQBandParameters parameters;
    double sampleRate = 44100.0;

    // Filter and envelope state per channel
    struct ChannelState
    {
        double envelopeState = 0.0;
        
        // Biquad state (Transposed Direct Form II)
        double z1 = 0.0;
        double z2 = 0.0;
    };
    
    std::vector<ChannelState> channelStates;
    double linkedEnvelopeState = 0.0;

    double attackCoef = 0.0;
    double releaseCoef = 0.0;

    // Precalculated filter base values
    double alpha = 0.0;
    double cosW0 = 0.0;
};

} // namespace ProEQ
