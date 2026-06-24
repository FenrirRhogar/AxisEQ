#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ProEQAudioProcessor::ProEQAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, juce::Identifier("ProEQParameters"),
            ProEQ::createParameterLayout())
{
    // Bind all 24 band parameter caches to the APVTS.
    // After this loop every FilterBandParameters struct holds 13 non-null
    // pointers into the APVTS atomic storage, ready for audio-thread reads.
    for (int i = 0; i < ProEQ::MaxBands; ++i)
        bandParams[static_cast<size_t>(i)].bindToAPVTS(apvts, i);
}

ProEQAudioProcessor::~ProEQAudioProcessor() = default;

//==============================================================================
const juce::String ProEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool   ProEQAudioProcessor::acceptsMidi()  const { return false; }
bool   ProEQAudioProcessor::producesMidi() const { return false; }
bool   ProEQAudioProcessor::isMidiEffect() const { return false; }
double ProEQAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  ProEQAudioProcessor::getNumPrograms()                        { return 1; }
int  ProEQAudioProcessor::getCurrentProgram()                     { return 0; }
void ProEQAudioProcessor::setCurrentProgram(int /*index*/)        {}
const juce::String ProEQAudioProcessor::getProgramName(int /*i*/) { return {}; }
void ProEQAudioProcessor::changeProgramName(int, const juce::String&) {}

bool ProEQAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ProEQAudioProcessor::createEditor()
{
    return new ProEQAudioProcessorEditor(*this);
}

//==============================================================================
bool ProEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // Accept only mono or stereo, and input must match output.
    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainOut != layouts.getMainInputChannelSet())
        return false;

    return true;
}

//==============================================================================
void ProEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    // Reset spectrum analyzer FIFOs so stale data from a previous session
    // does not bleed into the new playback context.
    for (int ch = 0; ch < 2; ++ch)
    {
        inputFifo[ch].prepare(samplesPerBlock);
        outputFifo[ch].prepare(samplesPerBlock);
    }

    // Prepare DSP processors
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    for (int i = 0; i < ProEQ::MaxBands; ++i)
    {
        leftStaticBands[i].prepare(sampleRate);
        rightStaticBands[i].prepare(sampleRate);
        dynamicBands[i].prepare(spec);
    }
}

void ProEQAudioProcessor::releaseResources() {}

//==============================================================================
void ProEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();

    // Clear any output channels that don't correspond to an input channel.
    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Capture pre-filter audio into the input spectrum analyzer FIFO.
    // juce::jmin guards against mono configurations where only channel 0 exists.
    const int channelsToPush = juce::jmin(totalNumInputChannels, 2);
    for (int ch = 0; ch < channelsToPush; ++ch)
        inputFifo[ch].push(buffer.getReadPointer(ch), numSamples);

    // ─────────────────────────────────────────────────────────────────────
    // Per-band DSP processing: Stereo, Mid, and Side Routing
    // ─────────────────────────────────────────────────────────────────────

    // 1. Update filter coefficients for all active bands
    for (int i = 0; i < ProEQ::MaxBands; ++i)
    {
        if (! bandParams[i].shouldProcess()) continue;
        
        bool useDynamic = bandParams[i].isDynEnabled() && bandParams[i].filterUsesGain();
        if (useDynamic)
        {
            ProEQ::DynamicEQBandParameters dParams;
            dParams.frequency = bandParams[i].getFrequency();
            dParams.q = bandParams[i].getQ();
            dParams.staticGainDb = bandParams[i].getGain();
            dParams.thresholdDb = bandParams[i].getDynThreshold();
            dParams.rangeDb = bandParams[i].getDynRange();
            dParams.attackMs = bandParams[i].getDynAttackMs();
            dParams.releaseMs = bandParams[i].getDynReleaseMs();
            dParams.ratio = 2.0; // Hardcoded default, can be expanded later
            dParams.useRms = false; // Peak detection
            dParams.linked = (bandParams[i].getRoutingMode() == ProEQ::RoutingMode::Stereo);
            dynamicBands[i].setParameters(dParams);
        }
        else
        {
            leftStaticBands[i].updateParameters(bandParams[i]);
            rightStaticBands[i].updateParameters(bandParams[i]);
        }
    }

    // 2. Process Audio
    const bool isStereo = (totalNumInputChannels == 2);

    if (isStereo)
    {
        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);
        
        for (int i = 0; i < ProEQ::MaxBands; ++i)
        {
            if (! bandParams[i].shouldProcess()) continue;
            
            auto routing = bandParams[i].getRoutingMode();
            bool useDynamic = bandParams[i].isDynEnabled() && bandParams[i].filterUsesGain();
            
            if (useDynamic)
            {
                if (routing == ProEQ::RoutingMode::Stereo)
                {
                    for (int s = 0; s < numSamples; ++s)
                    {
                        left[s] = dynamicBands[i].processSample(0, left[s]);
                        right[s] = dynamicBands[i].processSample(1, right[s]);
                    }
                }
                else if (routing == ProEQ::RoutingMode::MidOnly)
                {
                    for (int s = 0; s < numSamples; ++s)
                    {
                        float m = (left[s] + right[s]) * 0.5f;
                        float sd = (left[s] - right[s]) * 0.5f;
                        m = dynamicBands[i].processSample(0, m);
                        left[s] = m + sd;
                        right[s] = m - sd;
                    }
                }
                else // Side
                {
                    for (int s = 0; s < numSamples; ++s)
                    {
                        float m = (left[s] + right[s]) * 0.5f;
                        float sd = (left[s] - right[s]) * 0.5f;
                        sd = dynamicBands[i].processSample(0, sd);
                        left[s] = m + sd;
                        right[s] = m - sd;
                    }
                }
            }
            else
            {
                if (routing == ProEQ::RoutingMode::Stereo)
                {
                    for (int s = 0; s < numSamples; ++s)
                    {
                        left[s] = leftStaticBands[i].processSample(left[s]);
                        right[s] = rightStaticBands[i].processSample(right[s]);
                    }
                }
                else if (routing == ProEQ::RoutingMode::MidOnly)
                {
                    for (int s = 0; s < numSamples; ++s)
                    {
                        float m = (left[s] + right[s]) * 0.5f;
                        float sd = (left[s] - right[s]) * 0.5f;
                        m = leftStaticBands[i].processSample(m);
                        left[s] = m + sd;
                        right[s] = m - sd;
                    }
                }
                else // Side
                {
                    for (int s = 0; s < numSamples; ++s)
                    {
                        float m = (left[s] + right[s]) * 0.5f;
                        float sd = (left[s] - right[s]) * 0.5f;
                        sd = leftStaticBands[i].processSample(sd);
                        left[s] = m + sd;
                        right[s] = m - sd;
                    }
                }
            }
        }
    }
    else // Mono
    {
        float* channel = buffer.getWritePointer(0);
        for (int i = 0; i < ProEQ::MaxBands; ++i)
        {
            if (! bandParams[i].shouldProcess()) continue;
            
            if (bandParams[i].isDynEnabled() && bandParams[i].filterUsesGain())
            {
                for (int s = 0; s < numSamples; ++s)
                    channel[s] = dynamicBands[i].processSample(0, channel[s]);
            }
            else
            {
                for (int s = 0; s < numSamples; ++s)
                    channel[s] = leftStaticBands[i].processSample(channel[s]);
            }
        }
    }
    // ─────────────────────────────────────────────────────────────────────

    // Capture post-filter audio into the output spectrum analyzer FIFO.
    const int channelsToCapture = juce::jmin(totalNumOutputChannels, 2);
    for (int ch = 0; ch < channelsToCapture; ++ch)
        outputFifo[ch].push(buffer.getReadPointer(ch), numSamples);
}

//==============================================================================
void ProEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Serialize the entire APVTS state tree to XML → binary.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ProEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Deserialize binary → XML → ValueTree and replace the APVTS state.
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProEQAudioProcessor();
}
