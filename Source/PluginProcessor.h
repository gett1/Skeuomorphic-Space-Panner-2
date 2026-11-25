#pragma once
#include <JuceHeader.h>
#include "DSP/StereoScope.h"
#include "Neural/SpatialNetwork.h"

class GettoneSpacePannerProcessor : public juce::AudioProcessor
{
public:
    GettoneSpacePannerProcessor();
    ~GettoneSpacePannerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::ValueTree copyVersionedState();
    void restoreVersionedState(juce::ValueTree state);

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState treeState;

    void setSpatialGroup(juce::String);
    juce::String getSpatialGroup() const;
    juce::String getTrackName() const;
    void updateTrackProperties(const TrackProperties&) override;
    const std::shared_ptr<panner::SpatialTrack> spatialTrack;
    StereoScope scope;
    std::atomic<float> peakL{ 0.0f };
    std::atomic<float> peakR{ 0.0f };

private:
    enum ParameterIndex
    {
        x, y, z, roomSize, width, inputGain, outputGain, bypass, send,
        node1X, node1Y, node2X, node2Y, node3X, node3Y, node4X, node4Y,
        reflections, theme, roomMode, mix, absorb, diffusion, parameterCount
    };
    static constexpr std::array<const char*, parameterCount> parameterIds{
        "posX", "posY", "posZ", "revSize", "width", "inGain", "outGain", "bypass", "sendMode",
        "node1X", "node1Y", "node2X", "node2Y", "node3X", "node3Y", "node4X", "node4Y",
        "erLevel", "theme", "roomMode", "mix", "absorb", "diffusion"
    };
    using ParameterValues = std::array<float, parameterCount>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    ParameterValues readParameters() noexcept;
    bool shouldProcess(const ParameterValues&, bool hostBypass) const noexcept;
    void render(juce::AudioBuffer<float>&, bool hostBypass);
    void processWithMix(juce::AudioBuffer<float>&, const ParameterValues&);
    void processEffect(juce::AudioBuffer<float>&, const ParameterValues&);
    void publishOutput(const juce::AudioBuffer<float>&);

    std::array<const std::atomic<float>*, parameterCount> parameters{};
    ParameterValues defaults{};
    ParameterValues lastParameters{};
    juce::CriticalSection stateRestoreLock;
    std::atomic<unsigned> stateRestoreSequence{ 0 };
    juce::AudioBuffer<float> transitionDry;
    juce::AudioBuffer<float> mixDry;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix { 1.0f };
    int transitionLength = 1;
    int transitionPosition = 0;
    int scopeStride = 2;
    int samplesUntilRoomPrediction = 0;
    std::array<float, 6> roomInputs{};
    std::array<float, 10> roomPrediction{};
    bool hasRoomPrediction = false;
    std::array<juce::SmoothedValue<float>, 8> neuralTaps;
    mutable juce::CriticalSection groupLock;
    juce::String spatialGroup, trackName;
    float lastCutoff = -1.0f;
    bool hasRendered = false;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> elevationFilters;
    juce::dsp::ProcessorChain<juce::dsp::StateVariableTPTFilter<float>, juce::dsp::Reverb> fxChain;

    juce::dsp::Limiter<float> safetyLimiter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedX, smoothedY, smoothedZ;
    float currentSampleRate = 44100.0f;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;

    // 8 Valori lisciati per le coordinate X e Y dei 4 punti di riflessione
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 8> smoothedNodes;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GettoneSpacePannerProcessor)
};
