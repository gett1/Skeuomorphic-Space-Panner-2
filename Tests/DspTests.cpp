#include "PluginProcessor.h"
#if GETT1_TEST_LEGACY_REFERENCE
#include "LegacyProcessor.h"
#endif

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr std::array<double, 4> rates{ 44100.0, 48000.0, 88200.0, 96000.0 };
constexpr std::array<int, 5> blocks{ 64, 128, 256, 512, 1024 };

void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class Processor>
void set(Processor& processor, const char* id, float value)
{
    auto& parameter = *processor.treeState.getParameter(id);
    parameter.setValueNotifyingHost(parameter.convertTo0to1(value));
}

void fillSignal(juce::AudioBuffer<float>& buffer, int start = 0, float gain = 1.0f)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // Deterministic stereo input with transients and peaks above full scale.
        const float t = static_cast<float>(i + start);
        buffer.setSample(0, i, gain * (1.2f * std::sin(t * 0.015f) + 0.08f * std::sin(t * 0.6f)));
        buffer.setSample(1, i, gain * (1.1f * std::cos(t * 0.023f) + 0.12f * std::sin(t * 0.4f)));
    }
}

bool same(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int start = 0)
{
    const auto bytes = sizeof(float) * static_cast<size_t>(a.getNumSamples() - start);
    return std::memcmp(a.getReadPointer(0, start), b.getReadPointer(0, start), bytes) == 0
        && std::memcmp(a.getReadPointer(1, start), b.getReadPointer(1, start), bytes) == 0;
}

float magnitude(const juce::AudioBuffer<float>& buffer)
{
    return juce::jmax(buffer.getMagnitude(0, 0, buffer.getNumSamples()),
                      buffer.getMagnitude(1, 0, buffer.getNumSamples()));
}

void testPassThrough(double rate, int block)
{
    GettoneSpacePannerProcessor processor;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> input(2, block), output(2, block);
    fillSignal(input);
    processor.prepareToPlay(rate, block);
    processor.scope.setActive(true);

    output.makeCopyOf(input);
    processor.processBlock(output, midi);
    require(same(input, output), "Fresh Neutral instance altered the input");

    set(processor, "roomMode", 0.0f);
    set(processor, "posZ", 0.6f);
    for (int samples = 0; samples < static_cast<int>(rate); samples += block)
    {
        output.makeCopyOf(input);
        processor.processBlock(output, midi);
    }
    require(!same(input, output), "Legacy DSP stayed inactive");

    set(processor, "bypass", 1.0f);
    const int fadeLength = juce::roundToInt(rate * 0.010);
    for (int offset = 0; offset < fadeLength + 2 * block; offset += block)
    {
        output.makeCopyOf(input);
        processor.processBlock(output, midi);
        const int fadeRemaining = juce::jmax(0, fadeLength - offset);
        if (fadeRemaining < block)
            require(same(input, output, fadeRemaining), "Internal bypass failed byte-for-byte null test");
    }

    require(processor.peakL.load() == input.getMagnitude(0, 0, block), "Bypassed left meter is incorrect");
    require(processor.peakR.load() == input.getMagnitude(1, 0, block), "Bypassed right meter is incorrect");
    std::array<StereoScope::Frame, 256> scope;
    require(processor.scope.readLatest(scope), "Scope did not publish data during bypass");
    output.makeCopyOf(input);
    processor.processBlock(output, midi);
    require(processor.scope.readLatest(scope), "Scope did not publish new dry data");
    const auto last = scope.back();
    bool scopePairFound = false;
    for (int i = 0; i < block; ++i)
        scopePairFound |= last.left == input.getSample(0, i) && last.right == input.getSample(1, i);
    require(scopePairFound, "Scope did not publish a coherent dry stereo pair");

    set(processor, "bypass", 0.0f);
    for (int offset = 0; offset < static_cast<int>(rate / 2); offset += block)
    {
        output.clear();
        processor.processBlock(output, midi);
        require(magnitude(output) == 0.0f, "Stale reverb or early reflections returned after bypass");
    }

    for (int offset = 0; offset < fadeLength + block; offset += block)
    {
        output.makeCopyOf(input);
        processor.processBlockBypassed(output, midi);
        if (offset >= fadeLength)
            require(same(input, output), "Host bypass failed byte-for-byte null test");
    }
}

void testState()
{
    GettoneSpacePannerProcessor source;
    GettoneSpacePannerProcessor restored;
    set(source, "posX", -0.4f);
    set(source, "theme", 3.0f);
    set(source, "mix", 0.27f);
    juce::MemoryBlock binary;
    source.getStateInformation(binary);
    restored.setStateInformation(binary.getData(), static_cast<int>(binary.getSize()));
    require(restored.treeState.getRawParameterValue("roomMode")->load() == 1.0f, "V2 Neutral mode lost on restore");
    require(restored.treeState.getRawParameterValue("posX")->load() == source.treeState.getRawParameterValue("posX")->load(),
            "Position lost on restore");
    require(restored.treeState.getRawParameterValue("theme")->load() == 3.0f, "Theme lost on restore");

    require(restored.treeState.getRawParameterValue("mix")->load() == 0.27f, "Mix lost on restore");

    auto old = source.copyVersionedState();
    old.removeChild(old.getChildWithProperty("id", "mix"), nullptr);
    restored.restoreVersionedState(old.createCopy());
    require(restored.treeState.getRawParameterValue("mix")->load() == 1.0f, "Previous Alpha mix default lost");
    old.removeProperty("stateVersion", nullptr);
    old.removeChild(old.getChildWithProperty("id", "roomMode"), nullptr);
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(), binary);
    restored.setStateInformation(binary.getData(), static_cast<int>(binary.getSize()));
    require(restored.treeState.getRawParameterValue("roomMode")->load() == 0.0f, "V1 session did not migrate to Legacy");
    require(restored.treeState.getRawParameterValue("theme")->load() == 3.0f, "V1 theme lost on migration");

    set(restored, "roomMode", 1.0f);
    restored.restoreVersionedState(old.createCopy());
    require(restored.treeState.getRawParameterValue("roomMode")->load() == 0.0f, "V1 XML preset did not migrate to Legacy");
    require(static_cast<int>(restored.copyVersionedState()["stateVersion"]) == 3, "Saved version is incorrect");
}

void testMix(double rate, int block)
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> input(2, block), dry(2, block), wet(2, block), middle(2, block);

    for (bool send : { false, true })
    {
        GettoneSpacePannerProcessor zero, full, half;
        for (auto* processor : { &zero, &full, &half })
        {
            set(*processor, "roomMode", 0.0f);
            set(*processor, "posZ", 0.8f);
            set(*processor, "posX", -0.6f);
            set(*processor, "inGain", 10.0f);
            set(*processor, "sendMode", send ? 1.0f : 0.0f);
            processor->prepareToPlay(rate, block);
        }
        set(zero, "mix", 0.0f);
        set(half, "mix", 0.5f);

        const int transition = juce::roundToInt(rate * 0.02);
        for (int offset = 0; offset < rate * 0.35; offset += block)
        {
            fillSignal(input, offset);
            dry.makeCopyOf(input);
            wet.makeCopyOf(input);
            middle.makeCopyOf(input);
            zero.processBlock(dry, midi);
            full.processBlock(wet, midi);
            half.processBlock(middle, midi);
            require(send ? magnitude(dry) == 0.0f : same(input, dry),
                    "Mix 0 did not return exact dry audio or a silent send as appropriate");
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < block; ++sample)
                {
                    const auto expected = std::lerp(dry.getSample(channel, sample), wet.getSample(channel, sample), 0.5f);
                    require(std::abs(middle.getSample(channel, sample) - expected) < 1.0e-6f,
                            "Mix 50 does not blend the complete output in the proper proportion");
                }
        }

        set(zero, "mix", 1.0f);
        for (int offset = 0; offset < transition + block; offset += block)
        {
            fillSignal(input, offset);
            dry.makeCopyOf(input);
            wet.makeCopyOf(input);
            full.processBlock(wet, midi);
            zero.processBlock(dry, midi);
            if (offset >= transition && !send)
                require(same(wet, dry), "Automation did not converge to the original full wet output");
        }
    }
}

void testVariableBlocks()
{
    GettoneSpacePannerProcessor processor;
    juce::MidiBuffer midi;
    processor.prepareToPlay(96000.0, 64);
    for (const auto size : { 0, 1, 2048, 3, 1024, 5, 2048 })
    {
        juce::AudioBuffer<float> buffer(2, size);
        if (size > 0)
            fillSignal(buffer);
        set(processor, "bypass", size % 2 == 0 ? 1.0f : 0.0f);
        set(processor, "roomMode", 0.0f);
        processor.processBlock(buffer, midi);
        for (auto channel = 0; channel < 2 && size > 0; ++channel)
            for (int i = 0; i < size; ++i)
                require(std::isfinite(buffer.getSample(channel, i)), "Invalid output in variable-size block");
    }
}

void testTail(double rate)
{
    GettoneSpacePannerProcessor processor;
    juce::MidiBuffer midi;
    constexpr int block = 256;
    juce::AudioBuffer<float> buffer(2, block);
    float greatestAfterTail = 0.0f;
    for (int preset = 0; preset < 5; ++preset)
    {
        set(processor, "roomMode", 0.0f);
        set(processor, "posZ", 1.0f);
        set(processor, "erLevel", 1.0f);
        set(processor, "revSize", 1.5f);
        set(processor, "theme", static_cast<float>(preset));
        processor.prepareToPlay(rate, block);

        for (int i = 0; i < static_cast<int>(rate / 2); i += block)
        {
            buffer.clear();
            processor.processBlock(buffer, midi);
        }

        float responsePeak = 0.0f, latePeak = 0.0f, afterTail = 0.0f;
        const auto tail = static_cast<int>(std::ceil(rate * processor.getTailLengthSeconds()));
        for (int start = 0; start < tail + static_cast<int>(rate); start += block)
        {
            buffer.clear();
            if (start == 0)
                buffer.setSample(0, 0, 1.0f);
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const auto peak = std::abs(buffer.getSample(ch, i));
                    require(std::isfinite(peak), "Nonfinite impulse response");
                    responsePeak = juce::jmax(responsePeak, peak);
                    if (start + i >= tail)
                        afterTail = juce::jmax(afterTail, peak);
                    if (start + i >= static_cast<int>(rate * 0.15))
                        latePeak = juce::jmax(latePeak, peak);
                }
        }
        require(responsePeak > 0.001f && latePeak > 1.0e-6f, "Expected a direct response and a late tail");
        require(afterTail < 1.0e-4f, "Wet tail exceeds -80 dBFS beyond reported duration");
        greatestAfterTail = juce::jmax(greatestAfterTail, afterTail);
    }
    std::cout << "Tail " << rate << " Hz, " << processor.getTailLengthSeconds()
              << " s reported, largest sample thereafter " << greatestAfterTail << '\n';
}

#if GETT1_TEST_LEGACY_REFERENCE
void testLegacy(double rate, int block)
{
    for (int preset = 0; preset < 5; ++preset)
    {
        LegacyProcessor reference;
        GettoneSpacePannerProcessor current;
        set(reference, "theme", static_cast<float>(preset));

        juce::MemoryBlock oldState;
        reference.getStateInformation(oldState);
        current.setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
        reference.prepareToPlay(rate, block);
        current.prepareToPlay(rate, block);

        juce::AudioBuffer<float> a(2, block), b(2, block);
        juce::MidiBuffer midi;
        for (int step = 0; step < 80; ++step)
        {
            if (step == 8 || step == 20 || step == 52)
            {
                const auto direction = step == 20 ? -1.0f : 1.0f;
                auto automate = [&](auto& processor)
                {
                    set(processor, "posX", direction * 0.74f);
                    set(processor, "posY", direction * 0.63f);
                    set(processor, "posZ", step == 52 ? 0.1f : 0.67f);
                    set(processor, "node4X", direction > 0.0f ? 0.19f : 0.98f);
                    set(processor, "node3Y", direction > 0.0f ? 0.38f : 0.65f);
                    set(processor, "sendMode", step == 20 ? 1.0f : 0.0f);
                };
                automate(reference);
                automate(current);
            }
            fillSignal(a, step * block, 0.25f);
            b.makeCopyOf(a);
            reference.processBlock(a, midi);
            current.processBlock(b, midi);
            require(same(a, b), "Legacy regression differs from v1.1.0");
        }
    }
}

template <class Processor>
double benchmark(int instances, bool moving)
{
    constexpr int blockSize = 128;
    constexpr int iterations = 1500;
    std::vector<std::unique_ptr<Processor>> processors;
    for (int i = 0; i < instances; ++i)
    {
        auto processor = std::make_unique<Processor>();
        if constexpr (std::is_same_v<Processor, GettoneSpacePannerProcessor>)
            set(*processor, "roomMode", 0.0f);
        set(*processor, "posZ", 0.65f);
        processor->prepareToPlay(48000.0, blockSize);
        processors.push_back(std::move(processor));
    }

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> input(2, blockSize), work(2, blockSize);
    fillSignal(input, 0, 0.3f);
    auto run = [&](int count)
    {
        for (int block = 0; block < count; ++block)
            for (auto& processor : processors)
            {
                if (moving && block % 16 == 0)
                {
                    set(*processor, "posX", block % 32 == 0 ? 0.7f : -0.7f);
                    set(*processor, "node1Y", block % 32 == 0 ? 0.2f : 0.8f);
                }
                work.makeCopyOf(input);
                processor->processBlock(work, midi);
            }
    };

    run(150);
    std::array<double, 3> milliseconds{};
    for (auto& elapsed : milliseconds)
    {
        const auto start = std::chrono::steady_clock::now();
        run(iterations);
        elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    std::sort(milliseconds.begin(), milliseconds.end());
    return milliseconds[1];
}
#endif
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    try
    {
#if GETT1_TEST_LEGACY_REFERENCE
        if (argc > 1 && std::string_view(argv[1]) == "--benchmark")
        {
            std::cout << "Workload, instances, v1.1.0 ms, v2 alpha ms, speed-up\n";
            for (const bool moving : { false, true })
                for (const int count : { 1, 8, 16, 32 })
                {
                    const auto original = benchmark<LegacyProcessor>(count, moving);
                    const auto alpha = benchmark<GettoneSpacePannerProcessor>(count, moving);
                    std::cout << (moving ? "automated" : "static") << ", " << count << ", "
                              << original << ", " << alpha << ", " << (original / alpha) << "x\n";
                }
            return 0;
        }
#else
        juce::ignoreUnused(argc, argv);
#endif
        testState();
        testVariableBlocks();
        for (const auto rate : rates)
        {
            for (const auto block : blocks)
            {
                testPassThrough(rate, block);
                testMix(rate, block);
#if GETT1_TEST_LEGACY_REFERENCE
                testLegacy(rate, block);
#endif
            }
            testTail(rate);
        }
        std::cout << "DSP checks passed (" << rates.size() * blocks.size() << " rate/block pairs)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
