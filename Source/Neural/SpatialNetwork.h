#pragma once
#include <JuceHeader.h>
#include "Weights.h"
#include "../DSP/TrackAnalysis.h"
#include <memory>
#include <mutex>

namespace panner {
inline constexpr std::array roles{ "Kick", "Snare", "Hat", "Bass", "Vocal", "Music / FX" };

struct SpatialTrack
{
    using State = juce::AudioProcessorValueTreeState;
    using Room = std::array<float, 3>;
    explicit SpatialTrack(State& state) : parameters(&state) {}
    mutable std::mutex mutex;
    State* parameters; // Protected on processor teardown. Audio never touches the registry.
    TrackAnalysis analysis;
    juce::String name;
    bool locked = false;
    Room undo{}, suggestion{};
    bool hasSuggestion = false, hasUndo = false;
    int roleOverride = -1;
    const juce::Uuid id;
};

class SpatialNetwork
{
public:
    void join(const std::shared_ptr<SpatialTrack>& track, juce::String group)
    {
        std::lock_guard lock(mutex);
        std::erase_if(entries, [&](const Entry& e) { return e.track.expired() || e.track.lock() == track; });
        track->analysis.enabled.store(group.isNotEmpty(), std::memory_order_relaxed);
        if (group.isNotEmpty())
            entries.push_back({ track, std::move(group) });
    }

    auto peers(const juce::String& group)
    {
        std::vector<std::shared_ptr<SpatialTrack>> result;
        if (group.isEmpty())
            return result;
        std::lock_guard lock(mutex);
        for (const auto& entry : entries)
            if (entry.group == group)
                if (auto track = entry.track.lock())
                    result.push_back(std::move(track));
        return result;
    }

    static SpatialNetwork& instance()
    {
        static SpatialNetwork network;
        return network;
    }

    static void analyze(const std::vector<std::shared_ptr<SpatialTrack>>& tracks)
    {
        struct Input { std::shared_ptr<SpatialTrack> track; TrackAnalysis::Features features; std::array<float, 6> role; };
        std::vector<Input> active;
        std::array<float, 8> context{};
        for (const auto& track : tracks)
        {
            std::lock_guard lock(track->mutex);
            track->hasSuggestion = false;
            const auto features = track->analysis.read();
            if (!track->parameters || (features[0] <= 0.0f && track->roleOverride < 0))
                continue;
            auto role = neural::instrument.infer(features, true);
            if (track->roleOverride >= 0)
            {
                role.fill(0.0f);
                role[static_cast<std::size_t>(track->roleOverride)] = 1.0f;
            }
            for (std::size_t i = 0; i < role.size(); ++i)
                context[i] += role[i];
            context[6] += features[0];
            context[7] += features[4];
            active.push_back({ track, features, role });
        }
        if (active.empty())
            return;
        for (auto& value : context)
            value /= static_cast<float>(active.size());

        for (std::size_t i = 0; i < active.size(); ++i)
        {
            auto& item = active[i];
            std::array<float, 17> input{};
            std::copy(item.role.begin(), item.role.end(), input.begin());
            input[6] = item.features[0];
            input[7] = item.features[4];
            // Alternating ranks keep equivalent stems from collapsing to one point.
            input[8] = (i % 2 == 0) ? .15f + .08f * (i % 8) : .85f - .08f * (i % 8);
            std::copy(context.begin(), context.end(), input.begin() + 9);
            const auto position = neural::scene.infer(input);
            std::lock_guard lock(item.track->mutex);
            item.track->suggestion = { 2 * position[0] - 1, 2 * position[1] - 1, position[2] };
            item.track->hasSuggestion = true;
        }
    }

    static void apply(const std::vector<std::shared_ptr<SpatialTrack>>& tracks, float amount, bool undo)
    {
        constexpr std::array names{ "posX", "posY", "posZ" };
        for (const auto& track : tracks)
        {
            std::lock_guard lock(track->mutex);
            if (!track->parameters || (!undo && (track->locked || !track->hasSuggestion)) || (undo && !track->hasUndo))
                continue;
            for (std::size_t i = 0; i < names.size(); ++i)
            {
                auto* parameter = track->parameters->getParameter(names[i]);
                const auto current = parameter->convertFrom0to1(parameter->getValue());
                const auto target = undo ? track->undo[i] : std::lerp(current, track->suggestion[i], amount);
                if (!undo)
                    track->undo[i] = current;
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(target));
                parameter->endChangeGesture();
            }
            track->hasUndo = !undo;
        }
    }

private:
    struct Entry { std::weak_ptr<SpatialTrack> track; juce::String group; };
    mutable std::mutex mutex;
    std::vector<Entry> entries;
};
}
