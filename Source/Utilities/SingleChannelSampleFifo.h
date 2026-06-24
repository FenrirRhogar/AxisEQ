#pragma once

#include <JuceHeader.h>
#include <array>
#include <algorithm>

namespace ProEQ
{

/// A lock-free, single-producer / single-consumer FIFO for streaming audio
/// samples between threads (typically audio → GUI/analyzer).
///
/// Uses juce::AbstractFifo for wait-free index management backed by a
/// fixed-size std::array.  The capacity defaults to 48 000 samples
/// (~1 second at 48 kHz), which is more than enough for any practical
/// block size or FFT window.
template <typename SampleType, size_t Capacity = 48000>
class SingleChannelSampleFifo
{
public:
    SingleChannelSampleFifo() = default;

    /// Call from prepareToPlay to reset the FIFO state.
    void prepare(int /*samplesPerBlock*/) noexcept
    {
        buffer.fill(SampleType{});
        abstractFifo.reset();
    }

    // ─── Audio-thread side (producer) ───────────────────────────────────

    /// Push a contiguous block of samples into the FIFO.
    /// Safe to call from the real-time audio thread (wait-free, no allocation).
    void push(const SampleType* data, int numSamples) noexcept
    {
        const auto scope = abstractFifo.write(numSamples);

        if (scope.blockSize1 > 0)
            std::copy_n(data, scope.blockSize1,
                        buffer.data() + scope.startIndex1);

        if (scope.blockSize2 > 0)
            std::copy_n(data + scope.blockSize1, scope.blockSize2,
                        buffer.data() + scope.startIndex2);
    }

    // ─── GUI / analyzer-thread side (consumer) ──────────────────────────

    /// Pull up to @p maxSamples from the FIFO into @p dest.
    /// @returns the number of samples actually read (may be less than
    ///          @p maxSamples if fewer samples are available).
    int pull(SampleType* dest, int maxSamples) noexcept
    {
        const auto available = abstractFifo.getNumReady();
        const auto toPull    = std::min(available, maxSamples);

        if (toPull <= 0)
            return 0;

        const auto scope = abstractFifo.read(toPull);

        if (scope.blockSize1 > 0)
            std::copy_n(buffer.data() + scope.startIndex1, scope.blockSize1,
                        dest);

        if (scope.blockSize2 > 0)
            std::copy_n(buffer.data() + scope.startIndex2, scope.blockSize2,
                        dest + scope.blockSize1);

        return toPull;
    }

    /// @returns the number of samples currently available for reading.
    int getNumAvailable() const noexcept
    {
        return abstractFifo.getNumReady();
    }

private:
    juce::AbstractFifo                   abstractFifo { static_cast<int>(Capacity) };
    std::array<SampleType, Capacity>     buffer {};
};

} // namespace ProEQ
