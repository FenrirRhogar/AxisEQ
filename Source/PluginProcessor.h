#pragma once

#include <JuceHeader.h>
#include "Utilities/Constants.h"
#include "Utilities/ParameterLayout.h"
#include "Utilities/SingleChannelSampleFifo.h"
#include "DSP/FilterBand.h"
#include "DSP/BandProcessor.h"

//==============================================================================
/// AxisEQ audio processor — manages 24 independent parametric EQ bands,
/// a lock-free spectrum analyzer FIFO, and full APVTS state persistence.
class AxisEQAudioProcessor final : public juce::AudioProcessor
{
public:
    AxisEQAudioProcessor();
    ~AxisEQAudioProcessor() override;

    //==========================================================================
    // juce::AudioProcessor overrides
    //==========================================================================
    void   prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void   releaseResources() override;
    bool   isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void   processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool   hasEditor() const override;

    const  juce::String getName() const override;
    bool   acceptsMidi()  const override;
    bool   producesMidi() const override;
    bool   isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int    getNumPrograms() override;
    int    getCurrentProgram() override;
    void   setCurrentProgram(int index) override;
    const  juce::String getProgramName(int index) override;
    void   changeProgramName(int index, const juce::String& newName) override;

    void   getStateInformation(juce::MemoryBlock& destData) override;
    void   setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    // Public data — accessible by the editor
    //==========================================================================

    /// Value-tree-state that owns all 312 automatable parameters.
    juce::AudioProcessorValueTreeState apvts;

    /// Cached parameter pointers for every band, bound once at construction.
    std::array<AxisEQ::FilterBandParameters, AxisEQ::MaxBands> bandParams;

    /// Lock-free FIFOs feeding the spectrum analyzer.
    /// Index 0 = left channel, index 1 = right channel.
    AxisEQ::SingleChannelSampleFifo<float> inputFifo[2];
    AxisEQ::SingleChannelSampleFifo<float> outputFifo[2];

    /// Current playback sample rate (safe to read from any thread after prepare).
    double getSampleRate() const noexcept { return currentSampleRate; }

    /// Read the current dynamic gain reduction for GUI visual feedback.
    float getDynamicGainDb(int bandIndex) const noexcept 
    { 
        if (bandIndex >= 0 && bandIndex < AxisEQ::MaxBands)
            return bands[static_cast<size_t>(bandIndex)].getDynamicGainDb();
        return 0.0f;
    }

private:
    double currentSampleRate  = 44100.0;
    int    currentBlockSize   = 512;

    // Static EQ Processors (one per channel per band)
    std::array<AxisEQ::BandProcessor, AxisEQ::MaxBands> bands;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AxisEQAudioProcessor)
};
