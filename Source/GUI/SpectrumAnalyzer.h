#pragma once

#include <JuceHeader.h>
#include "../Utilities/Constants.h"
#include "../Utilities/SingleChannelSampleFifo.h"
#include <array>
#include <atomic>

namespace ProEQ
{

/// A background thread that computes FFTs for the spectrum analyzer,
/// keeping heavy DSP workloads off the message (GUI) thread.
class SpectrumAnalyzer : public juce::Thread
{
public:
    SpectrumAnalyzer(SingleChannelSampleFifo<float>* inFifos, 
                     SingleChannelSampleFifo<float>* outFifos);
    ~SpectrumAnalyzer() override;

    /// Called by the GUI thread to get the latest smoothed frequency magnitudes (in dB)
    /// Channel 0 = Left, 1 = Right
    /// isOutput false = Input, true = Output
    void getMagnitudes(int channel, bool isOutput, std::array<float, FFTSize / 2>& dest) const;

private:
    void run() override;

    SingleChannelSampleFifo<float>* inputFifos;
    SingleChannelSampleFifo<float>* outputFifos;

    juce::dsp::FFT fft { FFTOrder };
    juce::dsp::WindowingFunction<float> window { FFTSize, juce::dsp::WindowingFunction<float>::hann };

    // Working buffers for FFT computation
    std::array<float, FFTSize * 2> fftData;

    // Time-domain overlap buffers
    std::array<std::array<std::array<float, FFTSize>, 2>, 2> overlapBuffers;

    // Double-buffered output magnitudes (smoothed)
    // Dimension 1: Output/Input (0=Input, 1=Output)
    // Dimension 2: Channel (0=L, 1=R)
    // Dimension 3: Bins
    std::array<std::array<std::array<std::atomic<float>, FFTSize / 2>, 2>, 2> smoothedMagnitudes;
    
    // Internal state for exponential decay
    std::array<std::array<std::array<float, FFTSize / 2>, 2>, 2> currentMagnitudes;

    void processFifo(SingleChannelSampleFifo<float>& fifo, int channel, bool isOutput);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace ProEQ
