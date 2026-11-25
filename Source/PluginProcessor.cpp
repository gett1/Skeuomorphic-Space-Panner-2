#include "PluginProcessor.h"
#include "PluginEditor.h"

GettoneSpacePannerProcessor::GettoneSpacePannerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    treeState(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        parameters[i] = treeState.getRawParameterValue(parameterIds[i]);
        const auto& parameter = *treeState.getParameter(parameterIds[i]);
        defaults[i] = parameter.convertFrom0to1(parameter.getDefaultValue());
    }
    lastParameters = defaults;
}

GettoneSpacePannerProcessor::~GettoneSpacePannerProcessor() {}

const juce::String GettoneSpacePannerProcessor::getName() const { return "Skeuomorphic Space Panner Alpha"; }
bool GettoneSpacePannerProcessor::acceptsMidi() const { return false; }
bool GettoneSpacePannerProcessor::producesMidi() const { return false; }
bool GettoneSpacePannerProcessor::isMidiEffect() const { return false; }
double GettoneSpacePannerProcessor::getTailLengthSeconds() const
{
    // JUCE Reverb, roomSize = 1: max feedback = 0.98. Budget -80 dB per
    // comb path (including the damping pole), then early taps and all-pass decay.
    constexpr double maxCombDelay = 1640.0 / 44100.0;
    constexpr double maxDamping = 0.95 * 0.4;
    constexpr double maxEarlyDelay = (5.0 + 40.0 * 1.5 * 1.4142135623730951) / 1000.0;
    constexpr double allPassDelaySum = (556.0 + 441.0 + 341.0 + 225.0 + 4.0 * 23.0) / 44100.0;
    const double dampingDelay = maxDamping / ((1.0 - maxDamping) * 44100.0);
    return std::ceil(maxEarlyDelay
                     + (maxCombDelay + dampingDelay) * std::log(0.0001) / std::log(0.98)
                     + allPassDelaySum * std::ceil(std::log(0.0001) / std::log(0.5)));
}
int GettoneSpacePannerProcessor::getNumPrograms() { return 1; }
int GettoneSpacePannerProcessor::getCurrentProgram() { return 0; }
void GettoneSpacePannerProcessor::setCurrentProgram(int) {}
const juce::String GettoneSpacePannerProcessor::getProgramName(int) { return {}; }
void GettoneSpacePannerProcessor::changeProgramName(int, const juce::String&) {}
void GettoneSpacePannerProcessor::releaseResources() {}

bool GettoneSpacePannerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
bool GettoneSpacePannerProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* GettoneSpacePannerProcessor::createEditor() { return new GettoneSpacePannerEditor(*this); }

juce::ValueTree GettoneSpacePannerProcessor::copyVersionedState()
{
    auto state = treeState.copyState();
    state.setProperty("stateVersion", 3, nullptr);
    return state;
}

void GettoneSpacePannerProcessor::restoreVersionedState(juce::ValueTree state)
{
    if (!state.hasType("PARAMETERS"))
        return;

    const auto addIfMissing = [&state](const char* id, float value)
    {
        if (!state.getChildWithProperty("id", id).isValid())
        {
            juce::ValueTree parameter { "PARAM" };
            parameter.setProperty("id", id, nullptr);
            parameter.setProperty("value", value, nullptr);
            state.appendChild(parameter, nullptr);
        }
    };
    addIfMissing("roomMode", 0.0f);
    addIfMissing("mix", 1.0f);

    const juce::ScopedLock lock(stateRestoreLock);
    stateRestoreSequence.fetch_add(1);
    treeState.replaceState(state);
    stateRestoreSequence.fetch_add(1);
}

void GettoneSpacePannerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    copyXmlToBinary(*copyVersionedState().createXml(), destData);
}

void GettoneSpacePannerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        restoreVersionedState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout GettoneSpacePannerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("posX", "Pos X", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("posY", "Pos Y", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("posZ", "Pos Z (Depth)", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("revSize", "Room Size", 0.1f, 1.5f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("width", "Stereo Width", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("inGain", "In Gain", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outGain", "Out Gain", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("sendMode", "Send Mode", false));

    // 4 Punti di riflessione (Coordinate normalizzate da 0.0 a 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node1X", "Node 1 X", 0.0f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node1Y", "Node 1 Y", 0.0f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node2X", "Node 2 X", 0.0f, 1.0f, 0.9f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node2Y", "Node 2 Y", 0.0f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node3X", "Node 3 X", 0.0f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node3Y", "Node 3 Y", 0.0f, 1.0f, 0.9f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node4X", "Node 4 X", 0.0f, 1.0f, 0.9f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("node4Y", "Node 4 Y", 0.0f, 1.0f, 0.9f));

    // Livello mix delle Early Reflections
    params.push_back(std::make_unique<juce::AudioParameterFloat>("erLevel", "ER Level", 0.0f, 1.0f, 0.5f));

    juce::StringArray themes = { "Kawaii", "Dark Garage", "Tropical Beach", "Cosmic Void", "Vintage Wood Room" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("theme", "Vibe", themes, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "roomMode", 2 }, "Room Mode", juce::StringArray{ "Legacy", "Neutral" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mix", 3 }, "Mix", juce::NormalisableRange<float>{ 0.0f, 1.0f }, 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")
            .withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)); })
            .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue() / 100.0f; })));

    return { params.begin(), params.end() };
}

void GettoneSpacePannerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = (float)sampleRate;
    transitionLength = juce::jmax(1, juce::roundToInt(0.010 * sampleRate));
    transitionDry.setSize(2, transitionLength);
    mixDry.setSize(2, juce::jmax(1, samplesPerBlock));
    smoothedMix.reset(sampleRate, 0.020);
    hasRendered = false;
    scopeStride = juce::jmax(1, juce::roundToInt(sampleRate / 24000.0));
    lastCutoff = -1.0f;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    fxChain.prepare(spec);

    // Inizializza il nostro buffer custom (1 secondo di ritardo massimo in stereo)
    delayBuffer.setSize(getTotalNumOutputChannels(), (int)sampleRate + 2000);
    delayBuffer.clear();
    delayWritePosition = 0;

    for (auto& node : smoothedNodes) {
        node.reset(sampleRate, 0.15);
    }

    auto& airAbsorption = fxChain.template get<0>();
    airAbsorption.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    airAbsorption.setResonance(0.1f);

    for (auto& filter : elevationFilters) {
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency(juce::jmin(4500.0f, currentSampleRate * 0.49f));
        filter.setResonance(0.3f);
    }

    safetyLimiter.prepare(spec);
    safetyLimiter.setThreshold(-0.1f);
    safetyLimiter.setRelease(10.0f);

    smoothedX.reset(sampleRate, 0.15); smoothedY.reset(sampleRate, 0.15); smoothedZ.reset(sampleRate, 0.15);
}

void GettoneSpacePannerProcessor::reset()
{
    fxChain.reset();
    for (auto& filter : elevationFilters)
        filter.reset();
    safetyLimiter.reset();
    delayBuffer.clear();
    delayWritePosition = 0;
}

GettoneSpacePannerProcessor::ParameterValues GettoneSpacePannerProcessor::readParameters() noexcept
{
    // If a preset/session restore crosses this block boundary, reuse the last
    // complete snapshot. The audio thread never takes the state restore lock.
    const auto sequence = stateRestoreSequence.load();
    if (sequence % 2 != 0)
        return lastParameters;

    ParameterValues values{};
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = parameters[i]->load();
    if (sequence == stateRestoreSequence.load())
        lastParameters = values;
    return lastParameters;
}

bool GettoneSpacePannerProcessor::shouldProcess(const ParameterValues& values, bool hostBypass) const noexcept
{
    if (hostBypass || values[bypass] > 0.5f)
        return false;

    if (values[roomMode] < 0.5f)
        return true;

    for (size_t i = 0; i < roomMode; ++i)
        if (i != bypass && values[i] != defaults[i])
            return true;

    return false;
}

void GettoneSpacePannerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    render(buffer, false);
}

void GettoneSpacePannerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    render(buffer, true);
}

void GettoneSpacePannerProcessor::render(juce::AudioBuffer<float>& buffer, bool hostBypass)
{
    const auto samples = buffer.getNumSamples();
    if (samples == 0 || buffer.getNumChannels() != 2)
        return;

    juce::ScopedNoDenormals noDenormals;
    const auto values = readParameters();
    const bool processing = shouldProcess(values, hostBypass);
    if (!hasRendered)
    {
        transitionPosition = processing ? transitionLength : 0;
        smoothedMix.setCurrentAndTargetValue(values[mix]);
        hasRendered = true;
    }
    const int remaining = processing ? transitionLength - transitionPosition : transitionPosition;

    if (processing && remaining == 0)
    {
        processWithMix(buffer, values);
    }
    else if (remaining > 0)
    {
        const int fadeSamples = juce::jmin(samples, remaining);
        transitionDry.copyFrom(0, 0, buffer, 0, 0, fadeSamples);
        transitionDry.copyFrom(1, 0, buffer, 1, 0, fadeSamples);

        // During fade-out, samples after the transition stay untouched. A
        // stereo view uses AudioBuffer's inline channel-pointer storage.
        juce::AudioBuffer<float> effectBuffer(buffer.getArrayOfWritePointers(), 2,
                                              processing ? samples : fadeSamples);
        processWithMix(effectBuffer, values);

        for (int i = 0; i < fadeSamples; ++i)
        {
            transitionPosition += processing ? 1 : -1;
            const auto wet = static_cast<float>(transitionPosition) / static_cast<float>(transitionLength);
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto dry = transitionDry.getSample(channel, i);
                const auto effect = buffer.getSample(channel, i);
                buffer.setSample(channel, i, wet == 0.0f ? dry : std::lerp(dry, effect, wet));
            }
        }

        if (transitionPosition == 0)
            reset();
    }

    if (!processing && transitionPosition == 0)
        smoothedMix.setCurrentAndTargetValue(values[mix]);

    publishOutput(buffer);
}

void GettoneSpacePannerProcessor::publishOutput(const juce::AudioBuffer<float>& buffer)
{
    peakL.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    peakR.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()), std::memory_order_relaxed);
    scope.push(buffer, scopeStride);
}

void GettoneSpacePannerProcessor::processWithMix(juce::AudioBuffer<float>& buffer, const ParameterValues& values)
{
    smoothedMix.setTargetValue(values[mix]);
    if (!smoothedMix.isSmoothing() && smoothedMix.getCurrentValue() == 1.0f)
    {
        processEffect(buffer, values);
        return;
    }

    // SEND restricts the output to the processed return. MIX then controls the
    // return level, without summing a duplicate dry signal on the host's aux.
    const bool sendOnly = values[send] > 0.5f;
    const auto samples = buffer.getNumSamples();
    for (int start = 0; start < samples;)
    {
        const auto count = juce::jmin(samples - start, mixDry.getNumSamples());
        if (!sendOnly)
            for (int channel = 0; channel < 2; ++channel)
                mixDry.copyFrom(channel, 0, buffer, channel, start, count);

        juce::AudioBuffer<float> effect(buffer.getArrayOfWritePointers(), 2, start, count);
        processEffect(effect, values);

        auto* left = effect.getWritePointer(0);
        auto* right = effect.getWritePointer(1);
        const auto* dryLeft = mixDry.getReadPointer(0);
        const auto* dryRight = mixDry.getReadPointer(1);
        for (int i = 0; i < count; ++i)
        {
            const auto amount = smoothedMix.getNextValue();
            if (amount == 1.0f)
                continue;

            if (sendOnly)
            {
                left[i] *= amount;
                right[i] *= amount;
            }
            else
            {
                left[i] = std::lerp(dryLeft[i], left[i], amount);
                right[i] = std::lerp(dryRight[i], right[i], amount);
            }
        }
        start += count;
    }
}

void GettoneSpacePannerProcessor::processEffect(juce::AudioBuffer<float>& buffer, const ParameterValues& values)
{
    buffer.applyGain(juce::Decibels::decibelsToGain(values[inputGain]));
    const float sizeMultiplier = values[roomSize];
    const float stereoWidth = values[width];
    const int themeChoice = static_cast<int>(values[theme]);
    const bool sendMode = values[send] > 0.5f;
    const float erLevel = values[reflections];

    smoothedX.setTargetValue(values[x]);
    smoothedY.setTargetValue(values[y]);
    smoothedZ.setTargetValue(values[z]);
    const float currentZ = smoothedZ.getCurrentValue();
    for (size_t i = 0; i < smoothedNodes.size(); ++i)
        smoothedNodes[i].setTargetValue(values[node1X + i]);

    const int delaySize = delayBuffer.getNumSamples();
    auto* delayL = delayBuffer.getWritePointer(0);
    auto* delayR = delayBuffer.getWritePointer(1);
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const auto sampleCount = buffer.getNumSamples();
    const float samplesPerMs = currentSampleRate / 1000.0f;
    const float erDamping = 1.0f / (1.0f + sizeMultiplier * 1.5f);

    struct Reflection { float delay, gain; };
    std::array<Reflection, 4> taps{};
    float shelfGain = 0.0f;
    float panL = 0.0f, panR = 0.0f;
    auto updateGeometry = [&]
    {
        const float curX = smoothedX.getNextValue();
        const float curY = smoothedY.getNextValue();
        const float normSrcX = (curX + 1.0f) * 0.5f;
        const float normSrcY = (curY + 1.0f) * 0.5f;
        shelfGain = juce::jmap(curY, -1.0f, 1.0f, -1.0f, 1.5f);
        const float theta = juce::MathConstants<float>::pi / 4.0f * (curX + 1.0f);
        panL = juce::dsp::FastMathApproximations::cos(theta);
        panR = juce::dsp::FastMathApproximations::sin(theta);

        for (size_t i = 0; i < taps.size(); ++i)
        {
            const float dx = normSrcX - smoothedNodes[i * 2].getNextValue();
            const float dy = normSrcY - smoothedNodes[i * 2 + 1].getNextValue();
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float delayMs = 5.0f + (distance * 40.0f) * sizeMultiplier;
            taps[i] = { delayMs * samplesPerMs,
                        (1.0f - juce::jlimit(0.0f, 1.0f, distance * 0.7f)) * erDamping };
        }
    };

    const bool moving = smoothedX.isSmoothing() || smoothedY.isSmoothing()
        || std::any_of(smoothedNodes.begin(), smoothedNodes.end(), [](const auto& node) { return node.isSmoothing(); });
    if (!moving)
        updateGeometry();
    for (int sample = 0; sample < sampleCount && smoothedZ.isSmoothing(); ++sample)
        smoothedZ.getNextValue();

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        if (moving)
            updateGeometry();

        const float lDry = left[sample];
        const float rDry = right[sample];
        const float lHigh = elevationFilters[0].processSample(1, lDry);
        const float rHigh = elevationFilters[1].processSample(1, rDry);
        const float pannedL = (lDry + lHigh * shelfGain) * panL;
        const float pannedR = (rDry + rHigh * shelfGain) * panR;
        delayL[delayWritePosition] = pannedL;
        delayR[delayWritePosition] = pannedR;

        float earlyRefL = 0.0f;
        float earlyRefR = 0.0f;
        for (const auto& tap : taps)
        {
            float readPosition = delayWritePosition - tap.delay;
            if (readPosition < 0.0f)
                readPosition += delaySize;

            const int first = static_cast<int>(readPosition);
            const int second = first + 1 == delaySize ? 0 : first + 1;
            const float fraction = readPosition - first;
            const float tapL = delayL[first] + fraction * (delayL[second] - delayL[first]);
            const float tapR = delayR[first] + fraction * (delayR[second] - delayR[first]);

            earlyRefL += tapL * tap.gain * 0.25f * erLevel;
            earlyRefR += tapR * tap.gain * 0.25f * erLevel;
        }

        left[sample] = pannedL + earlyRefL;
        right[sample] = pannedR + earlyRefR;
        if (++delayWritePosition == delaySize)
            delayWritePosition = 0;
    }

    // 5. REVERBERO LATE
    juce::dsp::Reverb::Parameters revParams;
    float baseSize = 0.8f; float damping = 0.45f;
    if (themeChoice == 1) { baseSize = 0.65f; damping = 0.85f; }
    else if (themeChoice == 2) { baseSize = 0.9f; damping = 0.35f; }
    else if (themeChoice == 3) { baseSize = 0.98f; damping = 0.1f; }
    else if (themeChoice == 4) { baseSize = 0.5f; damping = 0.95f; }

    revParams.roomSize = juce::jlimit(0.0f, 1.0f, baseSize * sizeMultiplier);
    revParams.damping = damping;
    revParams.width = stereoWidth;

    revParams.dryLevel = sendMode ? 0.0f : (1.0f - (std::pow(currentZ, 0.8f) * 0.85f));
    revParams.wetLevel = std::pow(currentZ, 1.2f) * 0.85f;
    fxChain.template get<1>().setParameters(revParams);

    // 6. LOWPASS ATMOSFERICO
    float minCutoffZ = 16000.0f;
    if (themeChoice == 1) minCutoffZ = 9000.0f;
    else if (themeChoice == 2) minCutoffZ = 13000.0f;
    else if (themeChoice == 3) minCutoffZ = 18000.0f;
    else if (themeChoice == 4) minCutoffZ = 4500.0f;

    const auto cutoff = juce::jmin(juce::jmap(currentZ, 0.0f, 1.0f, 20000.0f, minCutoffZ),
                                   currentSampleRate * 0.499f);
    if (cutoff != lastCutoff)
    {
        fxChain.template get<0>().setCutoffFrequency(cutoff);
        lastCutoff = cutoff;
    }

    juce::dsp::AudioBlock<float> audioBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);
    fxChain.process(context);

    buffer.applyGain(juce::Decibels::decibelsToGain(values[outputGain]));
    safetyLimiter.process(context);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GettoneSpacePannerProcessor(); }
