// AxisEQ unit tests: run the AxisEQTests target; it exits non-zero on any failure.

#include <JuceHeader.h>
#include "DSP/BandDesign.h"
#include "DSP/BandProcessor.h"
#include "Utilities/ParameterLayout.h"
#include "Utilities/PresetManager.h"

using namespace AxisEQ;

namespace
{
    constexpr double fs = 48000.0;

    double dbAt(const BandDesign& d, double frequency)
    {
        return 20.0 * std::log10(d.magnitude(frequency, fs));
    }
}

//==============================================================================
class BandDesignTests : public juce::UnitTest
{
public:
    BandDesignTests() : juce::UnitTest("Band design", "AxisEQ") {}

    void runTest() override
    {
        const FilterSlope slopes[] = { FilterSlope::dB6, FilterSlope::dB12, FilterSlope::dB24,
                                       FilterSlope::dB48, FilterSlope::dB96 };

        beginTest("Cuts are Butterworth: -3 dB at the cutoff for every slope");
        for (auto slope : slopes)
            for (auto type : { FilterType::LowCut, FilterType::HighCut })
                expectWithinAbsoluteError(dbAt(designBand(type, slope, 1000.0, 0.7071, 0.0, fs), 1000.0), -3.01, 0.05);

        beginTest("Cuts match the Butterworth response two octaves out");
        for (auto slope : slopes)
        {
            // Digital Butterworth: the analogue response on the bilinear-warped frequency axis.
            const int order = filterOrderForSlope(slope);
            const auto warped = [] (double f) { return std::tan(juce::MathConstants<double>::pi * f / fs); };
            const auto expected = [order] (double ratio) { return -10.0 * std::log10(1.0 + std::pow(ratio, 2.0 * order)); };
            expectWithinAbsoluteError(dbAt(designBand(FilterType::LowCut, slope, 1000.0, 0.7071, 0.0, fs), 250.0),
                                      expected(warped(1000.0) / warped(250.0)), 0.1);
            expectWithinAbsoluteError(dbAt(designBand(FilterType::HighCut, slope, 1000.0, 0.7071, 0.0, fs), 4000.0),
                                      expected(warped(4000.0) / warped(1000.0)), 0.1);
        }

        beginTest("Cuts pass the rest of the spectrum untouched");
        expectWithinAbsoluteError(dbAt(designBand(FilterType::LowCut, FilterSlope::dB48, 100.0, 0.7071, 0.0, fs), 2000.0), 0.0, 0.05);
        expectWithinAbsoluteError(dbAt(designBand(FilterType::HighCut, FilterSlope::dB48, 8000.0, 0.7071, 0.0, fs), 200.0), 0.0, 0.05);

        beginTest("Resonance lifts the cutoff of steep cuts");
        {
            const auto resonant = designBand(FilterType::LowCut, FilterSlope::dB24, 1000.0, 2.0, 0.0, fs);
            double peak = -100.0;
            for (double f = 500.0; f < 4000.0; f *= 1.02)
                peak = std::max(peak, dbAt(resonant, f));
            expectGreaterThan(peak, 1.0);
        }

        beginTest("Bells reach their gain at the centre frequency");
        for (double gain : { -12.0, -3.0, 6.0, 18.0 })
            expectWithinAbsoluteError(dbAt(designBand(FilterType::Bell, FilterSlope::dB12, 1000.0, 1.0, gain, fs), 1000.0), gain, 0.01);

        beginTest("Shelves reach their gain on their own side and leave the other alone");
        expectWithinAbsoluteError(dbAt(designBand(FilterType::LowShelf, FilterSlope::dB12, 1000.0, 0.7071, 6.0, fs), 20.0), 6.0, 0.1);
        expectWithinAbsoluteError(dbAt(designBand(FilterType::LowShelf, FilterSlope::dB12, 1000.0, 0.7071, 6.0, fs), 20000.0), 0.0, 0.1);
        expectWithinAbsoluteError(dbAt(designBand(FilterType::HighShelf, FilterSlope::dB12, 1000.0, 0.7071, 6.0, fs), 20000.0), 6.0, 0.2);
        expectWithinAbsoluteError(dbAt(designBand(FilterType::HighShelf, FilterSlope::dB12, 1000.0, 0.7071, 6.0, fs), 20.0), 0.0, 0.1);

        beginTest("Tilt pivots around its frequency by half the gain each way");
        expectWithinAbsoluteError(dbAt(designBand(FilterType::TiltShelf, FilterSlope::dB12, 1000.0, 0.7071, 6.0, fs), 20.0), -3.0, 0.2);
        expectWithinAbsoluteError(dbAt(designBand(FilterType::TiltShelf, FilterSlope::dB12, 1000.0, 0.7071, 6.0, fs), 20000.0), 3.0, 0.3);

        beginTest("Designs stay stable up to the frequency limit");
        for (auto type : { FilterType::Bell, FilterType::LowCut, FilterType::HighShelf, FilterType::Notch })
        {
            const auto d = designBand(type, FilterSlope::dB96, 23900.0, 10.0, 12.0, fs);
            for (int i = 0; i < d.numStages; ++i)
                expectLessThan(std::abs(d.stages[static_cast<size_t>(i)].a2), 1.0);   // poles inside the unit circle
        }
    }
};

static BandDesignTests bandDesignTests;

//==============================================================================
/// A band's parameters as plain atomics, wired into FilterBandParameters like the APVTS would be.
struct TestBand
{
    std::atomic<float> frequency { 1000.0f }, gain { 0.0f }, q { 0.707f }, type { 0.0f }, slope { 1.0f },
        bypassed { 0.0f }, route { 0.0f }, active { 1.0f }, dynEnabled { 0.0f }, dynExtSc { 0.0f },
        threshold { -40.0f }, range { -12.0f }, attack { 1.0f }, release { 50.0f };
    FilterBandParameters params;

    TestBand()
    {
        params.frequency = &frequency;   params.gain = &gain;         params.q = &q;
        params.filterType = &type;       params.slope = &slope;       params.bypassed = &bypassed;
        params.routingMode = &route;     params.active = &active;     params.dynEnabled = &dynEnabled;
        params.dynExtSc = &dynExtSc;     params.dynThreshold = &threshold; params.dynRange = &range;
        params.dynAttack = &attack;      params.dynRelease = &release;
    }
};

/// Feeds sines at the given frequencies (amplitude each) through a band, returning the output.
static std::vector<float> run(BandProcessor& band, TestBand& t, std::initializer_list<double> freqs,
                              float amplitude, int numSamples, std::vector<float>* rightOut = nullptr)
{
    std::vector<float> left((size_t) numSamples), right((size_t) numSamples);
    for (int n = 0; n < numSamples; ++n)
    {
        double v = 0.0;
        for (double f : freqs)
            v += amplitude * std::sin(juce::MathConstants<double>::twoPi * f * n / fs);
        left[(size_t) n] = right[(size_t) n] = (float) v;
    }
    for (int start = 0; start < numSamples; start += 512)
    {
        const int len = std::min(512, numSamples - start);
        band.process(left.data() + start, right.data() + start, len, nullptr, nullptr, t.params);
    }
    if (rightOut != nullptr)
        *rightOut = right;
    return left;
}

static double rmsDb(const std::vector<float>& x, size_t from)
{
    double sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * x[i];
    return 10.0 * std::log10(sum / (double) (x.size() - from) + 1.0e-20);
}

class BandProcessorTests : public juce::UnitTest
{
public:
    BandProcessorTests() : juce::UnitTest("Band processor", "AxisEQ") {}

    void runTest() override
    {
        beginTest("A static bell applies its gain");
        {
            BandProcessor band; band.prepare(fs);
            TestBand t; t.gain = 6.0f;
            const auto out = run(band, t, { 1000.0 }, 0.1f, 24000);
            expectWithinAbsoluteError(rmsDb(out, 12000) - (20.0 * std::log10(0.1) - 3.01), 6.0, 0.1);
        }

        beginTest("Switching a band in and out never jumps the waveform");
        {
            BandProcessor band; band.prepare(fs);
            TestBand t; t.gain = 18.0f; t.q = 4.0f;
            std::vector<float> all;
            for (int block = 0; block < 40; ++block)
            {
                t.active = (block / 5) % 2 == 0 ? 1.0f : 0.0f;            // toggle every 5 blocks
                t.type = block % 10 == 3 ? 3.0f : 0.0f;                   // and flip type now and then
                std::vector<float> x(512);
                for (int n = 0; n < 512; ++n)
                    x[(size_t) n] = 0.3f * (float) std::sin(juce::MathConstants<double>::twoPi * 1000.0 * (block * 512 + n) / fs);
                band.process(x.data(), nullptr, 512, nullptr, nullptr, t.params);
                all.insert(all.end(), x.begin(), x.end());
            }
            float worst = 0.0f;
            for (size_t i = 1; i < all.size(); ++i)
                worst = std::max(worst, std::abs(all[i] - all[i - 1]));
            // A 1 kHz sine boosted 18 dB moves at most ~0.98 per sample; a click would far exceed it.
            expectLessThan(worst, 1.2f);
        }

        beginTest("Dynamics only react to the band's own frequency range");
        {
            BandProcessor band; band.prepare(fs);
            TestBand t; t.frequency = 100.0f; t.q = 1.0f; t.dynEnabled = 1.0f; t.threshold = -30.0f; t.range = -12.0f;
            run(band, t, { 8000.0 }, 0.5f, 24000);
            expectWithinAbsoluteError(band.getDynamicGainDb(), 0.0f, 0.5f);   // loud treble: no reaction
            run(band, t, { 100.0 }, 0.5f, 24000);
            expectLessThan(band.getDynamicGainDb(), -6.0f);                   // loud bass: it ducks
        }

        beginTest("Negative range cuts, positive range boosts");
        {
            for (float range : { -9.0f, 9.0f })
            {
                BandProcessor band; band.prepare(fs);
                TestBand t; t.dynEnabled = 1.0f; t.threshold = -40.0f; t.range = range;
                run(band, t, { 1000.0 }, 0.5f, 24000);
                expectWithinAbsoluteError(band.getDynamicGainDb(), range, 0.5f);
            }
        }

        beginTest("Linked stereo dynamics keep both channels identical");
        {
            BandProcessor band; band.prepare(fs);
            TestBand t; t.dynEnabled = 1.0f; t.threshold = -30.0f;
            std::vector<float> right;
            const auto left = run(band, t, { 1000.0 }, 0.5f, 12000, &right);
            float worst = 0.0f;
            for (size_t i = 0; i < left.size(); ++i)
                worst = std::max(worst, std::abs(left[i] - right[i]));
            expectLessThan(worst, 1.0e-6f);
        }

        beginTest("A dynamic low shelf still shapes like a shelf");
        {
            BandProcessor band; band.prepare(fs);
            TestBand t; t.type = 3.0f; t.frequency = 200.0f; t.gain = 6.0f; t.dynEnabled = 1.0f; t.threshold = 0.0f;
            const auto low = run(band, t, { 40.0 }, 0.05f, 48000);
            BandProcessor band2; band2.prepare(fs);
            const auto high = run(band2, t, { 5000.0 }, 0.05f, 48000);
            const double base = 20.0 * std::log10(0.05) - 3.01;
            expectWithinAbsoluteError(rmsDb(low, 24000) - base, 6.0, 0.3);    // below the shelf: +6
            expectWithinAbsoluteError(rmsDb(high, 24000) - base, 0.0, 0.3);   // above it: untouched
        }
    }
};

static BandProcessorTests bandProcessorTests;

//==============================================================================
/// The smallest processor that can own AxisEQ's parameter tree, for testing the preset manager.
class TestProcessor : public juce::AudioProcessor
{
public:
    TestProcessor() : apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {}

    const juce::String getName() const override { return "AxisEQ test"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

class PresetManagerTests : public juce::UnitTest
{
public:
    PresetManagerTests() : juce::UnitTest("Preset manager", "AxisEQ") {}

    void runTest() override
    {
        const auto tag = "AxisEQ test " + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64());
        const auto has = [] (PresetManager& m, const juce::String& name) {
            m.scanUserPresetsFromDisk();
            return m.getAllPresetNames().contains(name);
        };
        const auto remove = [] (PresetManager& m, const juce::String& name) {
            m.scanUserPresetsFromDisk();
            if (const int i = m.getAllPresetNames().indexOf(name); i >= 0)
                m.deleteUserPreset(i);
        };

        TestProcessor a, b;
        PresetManager first(a.apvts), second(b.apvts);

        beginTest("Two instances can both save without losing each other's presets");
        first.saveCurrentAsUserPreset(tag + " one");
        second.saveCurrentAsUserPreset(tag + " two");   // this instance never saw "one"
        expect(has(first, tag + " one"));
        expect(has(first, tag + " two"));

        beginTest("Deleting a preset removes only that preset");
        remove(second, tag + " one");
        expect(! has(first, tag + " one"));
        expect(has(first, tag + " two"));

        beginTest("Names with characters that are illegal in file names still save");
        const auto awkward = tag + " drums/bus: v2?";
        first.saveCurrentAsUserPreset(awkward);
        expect(has(second, awkward));

        remove(first, tag + " two");
        remove(first, awkward);
        expect(! has(second, tag + " two"));
    }
};

static PresetManagerTests presetManagerTests;

//==============================================================================
int main()
{
   #if JUCE_LINUX
    // Keep the preset tests out of the real ~/Documents.
    const auto home = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("AxisEQTestsHome");
    home.createDirectory();
    setenv("HOME", home.getFullPathName().toRawUTF8(), 1);
   #endif

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runTestsInCategory("AxisEQ");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;
    return failures > 0 ? 1 : 0;
}
