// AxisEQ DSP unit tests: run the AxisEQTests target; it exits non-zero on any failure.

#include <JuceHeader.h>
#include "DSP/BandDesign.h"

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
int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runTestsInCategory("AxisEQ");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;
    return failures > 0 ? 1 : 0;
}
