#pragma once
#include <JuceHeader.h>

namespace panner {
class TrackAnalysis
{
public:
    using Features = std::array<float, 5>;
    std::atomic<bool> enabled{false};

    void prepare(double sampleRate) noexcept
    {
        // Uniform feature clock across 44.1/48/88.2/96 kHz.
        step = 12000.0 / sampleRate;
        pole = static_cast<float>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 5200.0 / sampleRate));
        phase = 0;
        filteredLeft = filteredRight = 0;
        clear();
        for (auto& value : output)
            value.store(0.0f);
    }

    void push(const juce::AudioBuffer<float>& buffer, int inputChannels) noexcept
    {
        if (!enabled.load(std::memory_order_relaxed))
            return;
        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(inputChannels == 1 ? 0 : 1);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            filteredLeft += pole * (left[i] - filteredLeft);
            filteredRight += pole * (right[i] - filteredRight);
            phase += step;
            if (phase < 1.0)
                continue;
            phase -= 1.0;
            const float mono = 0.5f * (filteredLeft + filteredRight);
            const float side = 0.5f * (filteredLeft - filteredRight);
            const float change = mono - previous;
            power += mono * mono;
            diff += change * change;
            spread += side * side;
            peak = std::max(peak, std::abs(mono));
            crossings += static_cast<int>((mono > 0) != (previous > 0));
            previous = mono;
            if (++samples == 512)
            {
                // Keep the most energetic recent windows. Normalize exactly as in training.
                const float rms = std::sqrt(power / 512);
                if (rms > 0.002f)
                {
                    const Features frame{ rms * 4.0f, std::sqrt(diff / (power + 1e-8f)) * .5f,
                                          peak / (rms + 1e-6f) * .13f,
                                          crossings / 511.0f, std::sqrt(spread / (power + 1e-8f)) };
                    for (std::size_t f = 0; f < frame.size(); ++f)
                    {
                        auto old = output[f].load(std::memory_order_relaxed);
                        const auto clipped = std::clamp(frame[f], 0.0f, 1.0f);
                        output[f].store(old == 0.0f ? clipped : std::lerp(old, clipped, .3f), std::memory_order_relaxed);
                    }
                }
                clear();
            }
        }
    }

    Features read() const noexcept
    {
        Features frame{};
        for (std::size_t f = 0; f < frame.size(); ++f)
            frame[f] = output[f].load(std::memory_order_relaxed);
        return frame;
    }

private:
    void clear() noexcept { power = diff = spread = peak = previous = 0; crossings = samples = 0; }
    std::array<std::atomic<float>, 5> output{};
    float power{}, diff{}, spread{}, peak{}, previous{}, filteredLeft{}, filteredRight{}, pole{};
    int crossings{}, samples{};
    double phase{}, step{};
};
}
