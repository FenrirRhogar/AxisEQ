// AxisEQ parameter fuzz test: random values on every parameter while processing audio, at several
// sample rates and block sizes, like pluginval's "Fuzz parameters". Run the AxisEQFuzzTests target;
// it exits non-zero on a failure. Build it with -fsanitize=address to catch bad memory accesses.

#include <JuceHeader.h>
#include <cstdio>
#include <thread>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const int seed = argc > 1 ? juce::String (argv[1]).getIntValue() : 1;
    juce::Random random (seed);
    int failures = 0;

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const int blockSize : { 1, 32, 64, 512, 1024 })
        {
            std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
            plugin->setPlayConfigDetails (2, 2, sampleRate, blockSize);
            plugin->prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> audio (juce::jmax (2, plugin->getTotalNumInputChannels()), blockSize);
            juce::MidiBuffer midi;
            for (int round = 0; round < 300; ++round)
            {
                for (auto* p : plugin->getParameters())
                {
                    if (random.nextFloat() < 0.5f)
                        p->setValueNotifyingHost (random.nextFloat());

                    // What hosts (and pluginval) ask of a parameter besides its value.
                    const float v = random.nextFloat();
                    const auto text = p->getText (v, 1024);
                    juce::ignoreUnused (p->getValueForText (text), p->getText (v, 4), p->getCurrentValueAsText(),
                                        p->getName (32), p->getLabel(), p->getNumSteps(), p->getDefaultValue(),
                                        p->getAllValueStrings().size(), p->isDiscrete(), p->isBoolean());
                }

                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                    for (int n = 0; n < blockSize; ++n)
                        audio.setSample (ch, n, random.nextFloat() * 2.0f - 1.0f);

                plugin->processBlock (audio, midi);

                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < blockSize; ++n)
                        if (! std::isfinite (audio.getSample (ch, n)))
                        {
                            std::printf ("non-finite output at %g Hz, block %d, round %d\n", sampleRate, blockSize, round);
                            ++failures;
                            round = 1000;
                            break;
                        }
            }
            plugin->releaseResources();
        }
    }

    // As a host does with automation: parameters change on another thread while audio runs.
    {
        std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
        plugin->setPlayConfigDetails (2, 2, 48000.0, 256);
        plugin->prepareToPlay (48000.0, 256);
        std::atomic<bool> running { true };
        std::thread automation ([&]
        {
            juce::Random r (seed + 1000);
            while (running.load())
                for (auto* p : plugin->getParameters())
                    p->setValueNotifyingHost (r.nextFloat());
        });

        juce::AudioBuffer<float> audio (2, 256);
        juce::MidiBuffer midi;
        for (int round = 0; round < 20000; ++round)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < 256; ++n)
                    audio.setSample (ch, n, random.nextFloat() * 2.0f - 1.0f);
            plugin->processBlock (audio, midi);
        }
        running.store (false);
        automation.join();
    }

    std::printf ("seed %d: %s\n", seed, failures == 0 ? "passed" : "FAILED");
    return failures == 0 ? 0 : 1;
}
