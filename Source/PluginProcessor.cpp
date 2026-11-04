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

    bypass = apvts.getRawParameterValue("master_bypass");
    presetManager = std::make_unique<AxisEQ::PresetManager>(apvts);
}

juce::AudioProcessorParameter* AxisEQAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter("master_bypass");
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

// Factory presets double as host programs.
int AxisEQAudioProcessor::getNumPrograms()
{
    return juce::jmax(1, presetManager->getNumFactoryPresets());
}

int AxisEQAudioProcessor::getCurrentProgram()
{
    const int current = presetManager->getCurrentIndex();
    return current < presetManager->getNumFactoryPresets() ? current : 0;
}

void AxisEQAudioProcessor::setCurrentProgram(int index)
{
    // Some hosts select the current program again right after restoring a session;
    // reloading it would throw away the user's edits, so only real changes load.
    if (index != presetManager->getCurrentIndex() && index < presetManager->getNumFactoryPresets())
        presetManager->loadPreset(index);
}

const juce::String AxisEQAudioProcessor::getProgramName(int index)
{
    return index >= 0 && index < presetManager->getNumFactoryPresets() ? presetManager->getPreset(index).name
                                                                       : juce::String();
}

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

    dryBuffer.setSize(2, samplesPerBlock);
    bypassMix.reset(sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue(bypass->load() >= 0.5f ? 1.0f : 0.0f);
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

    const int dryChannels = juce::jmin(numMainChannels, 2);
    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize(2, numSamples, false, false, true);
    for (int ch = 0; ch < dryChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, mainBus, ch, 0, numSamples);

    for (int i = 0; i < AxisEQ::MaxBands; ++i)
        bands[static_cast<size_t>(i)].process(left, right, numSamples, scLeft, scRight,
                                              bandParams[static_cast<size_t>(i)]);

    bypassMix.setTargetValue(bypass->load() >= 0.5f ? 1.0f : 0.0f);
    if (bypassMix.isSmoothing() || bypassMix.getCurrentValue() > 0.0f)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float b = bypassMix.getNextValue();
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                auto* out = mainBus.getWritePointer(ch);
                out[n] += b * (dryBuffer.getSample(ch, n) - out[n]);
            }
        }
    }

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
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        presetManager->syncFromState();
    }

    // A switch keeps whatever raw value the host last sent (0.7 still reads as "on"), and
    // replaceState skips parameters whose on/off state is unchanged, so snap them to 0 or 1.
    for (auto* raw : getParameters())
        if (auto* toggle = dynamic_cast<juce::AudioParameterBool*>(raw))
            if (const float exact = toggle->get() ? 1.0f : 0.0f; std::abs(raw->getValue() - exact) > 1.0e-6f)
                toggle->setValueNotifyingHost(exact);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AxisEQAudioProcessor();
}
