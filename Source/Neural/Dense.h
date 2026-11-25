#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace panner::neural {
template <std::size_t Inputs, std::size_t Hidden, std::size_t Outputs>
struct Dense
{
    std::array<float, Inputs * Hidden> input;
    std::array<float, Hidden> bias;
    std::array<float, Hidden * Outputs> output;
    std::array<float, Outputs> outputBias;

    auto infer(const std::array<float, Inputs>& values, bool classification = false) const noexcept
    {
        auto hidden = bias;
        for (std::size_t i = 0; i < Inputs; ++i)
            for (std::size_t j = 0; j < Hidden; ++j)
                hidden[j] += values[i] * input[i * Hidden + j];

        auto result = outputBias;
        for (std::size_t i = 0; i < Hidden; ++i)
            for (std::size_t j = 0; j < Outputs; ++j)
                result[j] += std::max(0.0f, hidden[i]) * output[i * Outputs + j];

        if (classification)
        {
            const auto maximum = *std::max_element(result.begin(), result.end());
            float sum = 0;
            for (auto& value : result)
                sum += (value = std::exp(value - maximum));
            for (auto& value : result)
                value /= sum;
        }
        else
        {
            for (auto& value : result)
                value = 1.0f / (1.0f + std::exp(-value));
        }
        return result;
    }
};
}
