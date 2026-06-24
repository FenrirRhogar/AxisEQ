#include "SpectrumAnalyzer.h"
#include <algorithm>

namespace ProEQ
{

SpectrumAnalyzer::SpectrumAnalyzer(SingleChannelSampleFifo<float>* inFifos, 
                                   SingleChannelSampleFifo<float>* outFifos)
    : juce::Thread("SpectrumAnalyzerThread"),
      inputFifos(inFifos),
      outputFifos(outFifos)
{
    // Initialize atomic arrays to -100 dB and clear buffers
    for (int isOut = 0; isOut < 2; ++isOut)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            overlapBuffers[isOut][ch].fill(0.0f);
            currentMagnitudes[isOut][ch].fill(-100.0f);
            
            for (int i = 0; i < FFTSize / 2; ++i)
                smoothedMagnitudes[isOut][ch][i].store(-100.0f, std::memory_order_relaxed);
        }
    }

    // Start as a background thread
    startThread();
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopThread(5000); // Stop gracefully within 5 seconds
}

void SpectrumAnalyzer::getMagnitudes(int channel, bool isOutput, std::array<float, FFTSize / 2>& dest) const
{
    jassert(channel == 0 || channel == 1);
    const int outIdx = isOutput ? 1 : 0;
    
    for (int i = 0; i < FFTSize / 2; ++i)
    {
        dest[i] = smoothedMagnitudes[outIdx][channel][i].load(std::memory_order_relaxed);
    }
}

void SpectrumAnalyzer::run()
{
    while (! threadShouldExit())
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (!threadShouldExit()) processFifo(inputFifos[ch], ch, false);
            if (!threadShouldExit()) processFifo(outputFifos[ch], ch, true);
        }

        // Run at ~30-50 Hz (~20-30ms wait) to conserve CPU
        // The overlapping FFT logic ensures we maintain smooth resolution regardless
        wait(20);
    }
}

void SpectrumAnalyzer::processFifo(SingleChannelSampleFifo<float>& fifo, int channel, bool isOutput)
{
    const int outIdx = isOutput ? 1 : 0;
    auto& obuffer = overlapBuffers[outIdx][channel];
    
    // Buffer for new data from FIFO
    std::vector<float> newSamples;
    newSamples.resize(FFTSize);

    int numAvailable = fifo.getNumAvailable();
    if (numAvailable <= 0)
        return;

    // Pull max up to FFTSize at a time to prevent overflow
    int samplesPulled = fifo.pull(newSamples.data(), std::min(numAvailable, FFTSize));
    
    if (samplesPulled > 0)
    {
        // 1. Shift older data left in the overlap buffer
        if (samplesPulled < FFTSize)
        {
            std::copy(obuffer.begin() + samplesPulled, obuffer.end(), obuffer.begin());
            // 2. Append new samples to the end
            std::copy_n(newSamples.data(), samplesPulled, obuffer.begin() + (FFTSize - samplesPulled));
        }
        else
        {
            // If we pulled a full FFT block, just copy directly
            std::copy_n(newSamples.data(), FFTSize, obuffer.begin());
        }

        // 3. Copy into FFT working buffer
        std::copy(obuffer.begin(), obuffer.end(), fftData.begin());
        std::fill(fftData.begin() + FFTSize, fftData.end(), 0.0f); // Zero padding for complex parts

        // 4. Apply window
        window.multiplyWithWindowingTable(fftData.data(), FFTSize);

        // 5. Perform FFT
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // 6. Process magnitudes
        auto& currentMag = currentMagnitudes[outIdx][channel];
        auto& smoothedMag = smoothedMagnitudes[outIdx][channel];

        for (int i = 0; i < FFTSize / 2; ++i)
        {
            // Calculate dB
            float mag = fftData[i];
            float db = juce::Decibels::gainToDecibels(mag, -100.0f);

            // Apply exponential decay
            if (db > currentMag[i])
            {
                currentMag[i] = db; // Instant attack for fast visual peaks
            }
            else
            {
                currentMag[i] = db + (currentMag[i] - db) * AnalyzerDecayRate;
            }

            // Store atomically for the GUI thread
            smoothedMag[i].store(currentMag[i], std::memory_order_relaxed);
        }
    }
}

} // namespace ProEQ
