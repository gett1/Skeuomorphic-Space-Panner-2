#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Neural/ScenePanel.h"

class GettoneSpacePannerEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    GettoneSpacePannerEditor(GettoneSpacePannerProcessor&);
    ~GettoneSpacePannerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void timerCallback() override;

private:
    GettoneSpacePannerProcessor& audioProcessor;

    std::unique_ptr<juce::LookAndFeel_V4> hardwareLook;

    juce::ComboBox themeSelector;
    juce::ComboBox roomModeSelector;
    juce::ToggleButton bypassButton{ "BYPASS" };
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton{ "SAVE" };

    juce::Slider inGainSlider;
    juce::Slider outGainSlider;
    juce::Slider revSizeSlider;
    juce::Slider widthSlider;
    juce::Slider mixSlider, absorptionSlider, diffusionSlider;
    juce::TextButton sceneButton{ "GLOBAL MIX" };
    ScenePanel scenePanel;
    bool showScene = false;
    juce::ToggleButton sendModeButton{ "SEND MODE" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> themeAttachment, roomModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revSizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sendModeAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment, absorptionAttachment, diffusionAttachment;

    juce::File presetDir, alphaPresetDir;
    std::array<juce::Image, 5> backgrounds, centralArtwork;
    juce::Image screwArtwork;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // --- NUOVI COMPONENTI PER IL PANNELLO ADVANCED ---
    juce::TextButton advancedButton{ "ADVANCED" };
    bool isAdvancedPanelOpen = false;

    juce::Slider erLevelSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> erLevelAttachment;

    std::array<StereoScope::Frame, 256> scopeHistory{};
    std::array<juce::RangedAudioParameter*, 2> dragParameters{};
    int draggedNodeIndex = -1;

    juce::AffineTransform layoutTransform() const;
    void beginDragGesture(const juce::String& xId, const juce::String& yId);
    void endDragGesture();
    void updateParametersFromMouse(juce::Point<float> mousePos);
    void refreshPresetList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GettoneSpacePannerEditor)
};
