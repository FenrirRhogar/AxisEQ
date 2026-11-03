#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AxisEQAudioProcessor::AxisEQAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, juce::Identifier("AxisEQParameters"),
            AxisEQ::createParameterLayout())
{
    // Bind all 24 band parameter caches to the APVTS.
    // After this loop every FilterBandParameters struct holds 13 non-null
    // pointers into the APVTS atomic storage, ready for audio-thread reads.
    for (int i = 0; i < AxisEQ::MaxBands; ++i)
        bandParams[static_cast<size_t>(i)].bindToAPVTS(apvts, i);
}

AxisEQAudioProcessor::~AxisEQAudioProcessor() = default;

//==============================================================================
const juce::String AxisEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool   AxisEQAudioProcessor::acceptsMidi()  const { return false; }
bool   AxisEQAudioProcessor::producesMidi() const { return false; }
bool   AxisEQAudioProcessor::isMidiEffect() const { return false; }
double AxisEQAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  AxisEQAudioProcessor::getNumPrograms()                        { return 1; }
int  AxisEQAudioProcessor::getCurrentProgram()                     { return 0; }
void AxisEQAudioProcessor::setCurrentProgram(int /*index*/)        {}
const juce::String AxisEQAudioProcessor::getProgramName(int /*i*/) { return {}; }
void AxisEQAudioProcessor::changeProgramName(int, const juce::String&) {}

bool AxisEQAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AxisEQAudioProcessor::createEditor()
{
    return new AxisEQAudioProcessorEditor(*this);
}

//==============================================================================
bool AxisEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // Accept only mono or stereo, and input must match output.
    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainOut != layouts.getMainInputChannelSet())
        return false;

    // Optional Sidechain
    const auto& sidechain = layouts.getChannelSet(true, 1);
    if (sidechain != juce::AudioChannelSet::disabled() &&
        sidechain != juce::AudioChannelSet::mono() &&
        sidechain != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void AxisEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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

    for (auto& band : bands)
        band.prepare(sampleRate);
}

void AxisEQAudioProcessor::releaseResources() {}

//==============================================================================
void AxisEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();

    // ─────────────────────────────────────────────────────────────────────
    // Fetch sidechain pointers
    // ─────────────────────────────────────────────────────────────────────
    const bool hasSidechain = (getBusCount(true) > 1) && (getChannelCountOfBus(true, 1) > 0);
    const float* scLeft = nullptr;
    const float* scRight = nullptr;
    if (hasSidechain)
    {
        auto scBus = getBusBuffer(buffer, true, 1);
        if (scBus.getNumChannels() >= 2)
        {
            scLeft = scBus.getReadPointer(0);
            scRight = scBus.getReadPointer(1);
        }
        else if (scBus.getNumChannels() == 1)
        {
            scLeft = scBus.getReadPointer(0);
            scRight = scLeft; // Duplicate mono sidechain
        }
    }
    
    // We only process the main bus channels, but we pass sidechain samples to DynamicEQ.
    auto mainBus = getBusBuffer(buffer, true, 0);
    const int numMainChannels = mainBus.getNumChannels();

    // Clear any output channels that don't correspond to an input channel.
    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Capture pre-filter audio into the input spectrum analyzer FIFO.
    // juce::jmin guards against mono configurations where only channel 0 exists.
    const int channelsToPush = juce::jmin(numMainChannels, 2);
    for (int ch = 0; ch < channelsToPush; ++ch)
        inputFifo[ch].push(mainBus.getReadPointer(ch), numSamples);

    // ─────────────────────────────────────────────────────────────────────
    // Bands in series. Each handles its own routing, smoothing, crossfades and dynamics.
    // ─────────────────────────────────────────────────────────────────────
    float* left  = mainBus.getWritePointer(0);
    float* right = numMainChannels > 1 ? mainBus.getWritePointer(1) : nullptr;

    for (int i = 0; i < AxisEQ::MaxBands; ++i)
        bands[static_cast<size_t>(i)].process(left, right, numSamples, scLeft, scRight,
                                              bandParams[static_cast<size_t>(i)]);

    // Capture post-filter audio into the output spectrum analyzer FIFO.
    const int channelsToCapture = juce::jmin(numMainChannels, 2);
    for (int ch = 0; ch < channelsToCapture; ++ch)
        outputFifo[ch].push(mainBus.getReadPointer(ch), numSamples);
}

//==============================================================================
void AxisEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Serialize the entire APVTS state tree to XML → binary.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AxisEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Deserialize binary → XML → ValueTree and replace the APVTS state.
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AxisEQAudioProcessor();
}
