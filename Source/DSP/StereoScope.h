#pragma once

#include <JuceHeader.h>

// One producer (audio thread) and one consumer (editor message thread).
class StereoScope
{
public:
    struct Frame { float left = 0.0f, right = 0.0f; };

    void setActive(bool active) noexcept { enabled.store(active, std::memory_order_relaxed); }

    void push(const juce::AudioBuffer<float>& buffer, int stride) noexcept
    {
        if (!enabled.load(std::memory_order_relaxed) || offset >= buffer.getNumSamples())
        {
            offset = juce::jmax(0, offset - buffer.getNumSamples());
            return;
        }

        const auto samples = buffer.getNumSamples();
        const auto count = 1 + (samples - 1 - offset) / stride;
        auto slots = fifo.write(count);
        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(1);
        auto sample = offset;
        slots.forEach([&](int index)
        {
            frames[static_cast<size_t>(index)] = { left[sample], right[sample] };
            sample += stride;
        });
        // Retain the decimation phase across variable-size blocks and dropped frames.
        offset = (offset + count * stride) - samples;
    }

    template <size_t Size>
    bool readLatest(std::array<Frame, Size>& history) noexcept
    {
        auto slots = fifo.read(fifo.getNumReady());
        const auto count = slots.blockSize1 + slots.blockSize2;
        if (count == 0)
            return false;

        int drop = juce::jmax(0, count - static_cast<int>(Size));
        const auto keep = static_cast<int>(Size) - juce::jmin(count, static_cast<int>(Size));
        std::rotate(history.begin(), history.end() - keep, history.end());
        auto write = history.begin() + keep;
        slots.forEach([&](int index)
        {
            if (drop > 0)
                --drop;
            else
                *write++ = frames[static_cast<size_t>(index)];
        });
        return true;
    }

private:
    std::array<Frame, 2049> frames{};
    juce::AbstractFifo fifo{ static_cast<int>(frames.size()) };
    std::atomic<bool> enabled{ false };
    int offset = 0;
};
