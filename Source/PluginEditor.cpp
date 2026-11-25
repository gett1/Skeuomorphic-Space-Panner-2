#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

#include "Gett1Cat.h"

namespace {
const juce::Colour ivory(0xfff0e3ca);
const juce::Colour brass(0xffd5b479);

constexpr auto artworkFolder = "Skeuomorphic Space Panner Alpha Artwork";

juce::File findArtwork()
{
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto executableDirectory = executable.getParentDirectory();
    const auto contents = executableDirectory.getParentDirectory();
    const auto bundleDirectory = contents.getParentDirectory().getParentDirectory();
    const auto documents = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto packaged = contents.getFileName() == "Contents"
        ? contents.getChildFile("Resources").getChildFile(artworkFolder) : executableDirectory.getChildFile(artworkFolder);

    for (const auto& path : { packaged, bundleDirectory.getChildFile(artworkFolder),
                              bundleDirectory.getParentDirectory().getChildFile(artworkFolder),
                              documents.getChildFile(artworkFolder) })
        if (path.isDirectory())
            return path;

    return {};
}

juce::Image loadArtwork(const juce::File& folder, const char* filename, const char* fallback, int fallbackSize)
{
    if (folder.isDirectory())
        if (auto image = juce::ImageCache::getFromFile(folder.getChildFile(filename)); image.isValid())
            return image;

    return juce::ImageCache::getFromMemory(fallback, fallbackSize);
}

juce::Typeface::Ptr pixelTypeface()
{
    static const auto face = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PixelifySans_ttf, BinaryData::PixelifySans_ttfSize);
    return face;
}

juce::Font pixel(float height)
{
    return juce::Font(juce::FontOptions(pixelTypeface()).withHeight(height));
}

void drawFaceplate(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(0xff100e0c).withAlpha(0.75f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 4.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff383730), bounds.getTopLeft(),
                                          juce::Colour(0xff1c1e1b), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(ivory.withAlpha(0.18f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
}

class HardwareLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    HardwareLookAndFeel()
    {
        setDefaultSansSerifTypeface(pixelTypeface());
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff242622));
        setColour(juce::PopupMenu::textColourId, ivory);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff474337));
        setColour(juce::PopupMenu::highlightedTextColourId, ivory);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return pixel(juce::jlimit(10.0f, 20.0f, box.getHeight() * 0.55f));
    }

    juce::Font getPopupMenuFont() override { return pixel(15.0f); }

    juce::Font getTextButtonFont(juce::TextButton&, int height) override
    {
        return pixel(juce::jlimit(10.0f, 20.0f, height * 0.53f));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position,
                          float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto centre = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
        const auto radius = 0.37f * (float) juce::jmin(w, h);
        const auto angle = startAngle + position * (endAngle - startAngle);
        auto point = [&](float length, float radians) {
            return centre + juce::Point<float>(std::sin(radians), -std::cos(radians)) * length;
        };
        auto disk = [&](float scale) {
            return juce::Rectangle<float>(radius * scale * 2.0f, radius * scale * 2.0f).withCentre(centre);
        };

        for (int tick = 0; tick <= 16; ++tick)
        {
            const auto tickAngle = juce::jmap((float) tick / 16.0f, startAngle, endAngle);
            const auto major = tick % 4 == 0;
            g.setColour(brass.withAlpha(major ? 0.75f : 0.35f));
            g.drawLine({ point(radius * 1.09f, tickAngle), point(radius * (major ? 1.25f : 1.18f), tickAngle) },
                       major ? 1.15f : 0.7f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.72f));
        g.fillEllipse(disk(1.03f).translated(0.0f, radius * 0.13f));
        const auto rim = disk(1.0f);
        juce::ColourGradient rimGradient(ivory, rim.getTopLeft(), juce::Colour(0xff60472b), rim.getBottomRight(), false);
        rimGradient.addColour(0.47, brass);
        g.setGradientFill(rimGradient);
        g.fillEllipse(rim);

        juce::Path grip;
        for (int tooth = 0; tooth < 96; ++tooth)
        {
            const auto radians = juce::MathConstants<float>::twoPi * (float) tooth / 96.0f;
            const auto p = point(radius * (tooth % 4 < 2 ? 0.92f : 0.81f), radians);
            if (tooth == 0) grip.startNewSubPath(p); else grip.lineTo(p);
        }
        grip.closeSubPath();
        g.setColour(juce::Colour(0xff272b25));
        g.fillPath(grip);

        const auto cap = disk(0.76f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff55584b), cap.getTopLeft(),
                                              juce::Colour(0xff20241f), cap.getBottomRight(), false));
        g.fillEllipse(cap);
        g.setColour(juce::Colour(0xff10130f));
        g.drawEllipse(cap, 1.0f);
        const juce::Line<float> pointer(point(radius * 0.31f, angle), point(radius * 0.89f, angle));
        g.drawLine(pointer, juce::jmax(2.8f, radius * 0.21f));
        g.setColour(ivory);
        g.drawLine(pointer, juce::jmax(1.5f, radius * 0.12f));
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.drawLine({ point(radius * 0.82f, angle), point(radius * 0.91f, angle) }, 1.5f);
    }
};
}

GettoneSpacePannerEditor::GettoneSpacePannerEditor(GettoneSpacePannerProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), hardwareLook(std::make_unique<HardwareLookAndFeel>())
{
    setLookAndFeel(hardwareLook.get());
    const auto folder = findArtwork();
    backgrounds = {
        loadArtwork(folder, "wood_kawaii.jpg", BinaryData::wood_kawaii_jpg, BinaryData::wood_kawaii_jpgSize),
        loadArtwork(folder, "wood_garage.jpg", BinaryData::wood_garage_jpg, BinaryData::wood_garage_jpgSize),
        loadArtwork(folder, "wood_beach.jpg", BinaryData::wood_beach_jpg, BinaryData::wood_beach_jpgSize),
        loadArtwork(folder, "wood_cosmic.jpg", BinaryData::wood_cosmic_jpg, BinaryData::wood_cosmic_jpgSize),
        loadArtwork(folder, "wood_vintage.jpg", BinaryData::wood_vintage_jpg, BinaryData::wood_vintage_jpgSize)
    };
    centralArtwork = {
        loadArtwork(folder, "anime.png", BinaryData::anime_png, BinaryData::anime_pngSize),
        loadArtwork(folder, "graffiti.png", BinaryData::graffiti_png, BinaryData::graffiti_pngSize),
        loadArtwork(folder, "beach.png", BinaryData::beach_png, BinaryData::beach_pngSize),
        loadArtwork(folder, "cosmic.png", BinaryData::cosmic_png, BinaryData::cosmic_pngSize),
        loadArtwork(folder, "tree.png", BinaryData::tree_png, BinaryData::tree_pngSize)
    };
    screwArtwork = loadArtwork(folder, "screw.png", BinaryData::screw_png, BinaryData::screw_pngSize);
    themeSelector.addItem("Kawaii", 1);
    themeSelector.addItem("Dark Garage", 2);
    themeSelector.addItem("Tropical Beach", 3);
    themeSelector.addItem("Cosmic Void", 4);
    themeSelector.addItem("Vintage Wood Room", 5);
    addAndMakeVisible(themeSelector);
    themeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.treeState, "theme", themeSelector);

    roomModeSelector.addItem("Legacy", 1);
    roomModeSelector.addItem("Neutral", 2);
    roomModeSelector.setTooltip("Neutral passes the input through while all spatial controls are at their defaults.");
    addAndMakeVisible(roomModeSelector);
    roomModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.treeState, "roomMode", roomModeSelector);

    bypassButton.setButtonText("BYPASS");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.treeState, "bypass", bypassButton);

    presetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("GettoneSpacePanner_Presets");
    alphaPresetDir = presetDir.getSiblingFile("GettoneSpacePanner_Alpha_Presets");
    alphaPresetDir.createDirectory();

    addAndMakeVisible(presetSelector);
    refreshPresetList();

    presetSelector.onChange = [this] {
        if (presetSelector.getSelectedId() > 1) {
            const auto name = presetSelector.getText() + ".xml";
            auto f = alphaPresetDir.getChildFile(name);
            if (!f.existsAsFile())
                f = presetDir.getChildFile(name);
            if (f.existsAsFile()) {
                std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(f);
                if (xml != nullptr) audioProcessor.restoreVersionedState(juce::ValueTree::fromXml(*xml));
            }
        }
        };

    savePresetButton.setButtonText("SAVE");
    addAndMakeVisible(savePresetButton);
    savePresetButton.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Save Preset", alphaPresetDir, "*.xml");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file != juce::File{}) {
                    auto state = audioProcessor.copyVersionedState();
                    std::unique_ptr<juce::XmlElement> xml(state.createXml());
                    xml->writeTo(file);
                    refreshPresetList();
                    presetSelector.setText(file.getFileNameWithoutExtension(), juce::dontSendNotification);
                }
            });
        };

    auto setupSlider = [this](juce::Slider& s, double def) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled(true, false, this);
        s.setDoubleClickReturnValue(true, def);
        addChildComponent(s);
        s.setVisible(true);
        };

    setupSlider(inGainSlider, 0.0); inGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "inGain", inGainSlider);
    setupSlider(outGainSlider, 0.0); outGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "outGain", outGainSlider);
    setupSlider(revSizeSlider, 1.0); revSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "revSize", revSizeSlider);
    setupSlider(widthSlider, 1.0); widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "width", widthSlider);

    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
    mixSlider.setName("Mix");
    mixSlider.setTooltip("Mix: original input to complete effect. In SEND it controls the wet return level.");
    mixSlider.setDoubleClickReturnValue(true, 1.0);
    addAndMakeVisible(mixSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "mix", mixSlider);

    sendModeButton.setAlpha(0.0f);
    addChildComponent(sendModeButton);
    sendModeButton.setVisible(true);
    sendModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.treeState, "sendMode", sendModeButton);

    advancedButton.setButtonText("ADV");
    advancedButton.setAlpha(0.0f);
    addAndMakeVisible(advancedButton);
    advancedButton.onClick = [this] {
        const auto scale = layoutTransform().mat00;
        isAdvancedPanelOpen = !isAdvancedPanelOpen;
        const auto targetWidth = isAdvancedPanelOpen ? 1020 : 680;
        const auto fit = juce::jmin(scale, 1200.0f / (float) targetWidth);
        setSize(juce::roundToInt((float) targetWidth * fit), getHeight());
        resized();
        repaint();
        };

    // --- NUOVO: SLIDER ORIZZONTALE ER LEVEL ---
    erLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    erLevelSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    erLevelSlider.setPopupDisplayEnabled(true, false, this);
    addChildComponent(erLevelSlider);
    erLevelSlider.setVisible(true);
    erLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.treeState, "erLevel", erLevelSlider);

    setResizable(true, true);
    setResizeLimits(500, 400, 1200, 960);
    setSize(680, 520);

    startTimerHz(30);
}

GettoneSpacePannerEditor::~GettoneSpacePannerEditor() {
    stopTimer();
    endDragGesture();
    audioProcessor.scope.setActive(false);
    setLookAndFeel(nullptr);
}

void GettoneSpacePannerEditor::timerCallback()
{
    const bool showing = isShowing();
    audioProcessor.scope.setActive(showing);
    if (showing)
    {
        audioProcessor.scope.readLatest(scopeHistory);
        repaint();
    }
}

void GettoneSpacePannerEditor::refreshPresetList() {
    presetSelector.clear();
    presetSelector.addItem("Default", 1);
    juce::StringArray names;
    for (const auto& folder : { alphaPresetDir, presetDir })
        for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.xml"))
            names.addIfNotAlreadyThere(file.getFileNameWithoutExtension());
    names.sort(true);
    presetSelector.addItemList(names, 2);
    if (presetSelector.getSelectedId() == 0) presetSelector.setSelectedId(1, juce::dontSendNotification);
}

void GettoneSpacePannerEditor::paint(juce::Graphics& g)
{
    const auto themeChoice = juce::jlimit(0, 4, static_cast<int>(audioProcessor.treeState.getRawParameterValue("theme")->load()));
    bool isBypassed = audioProcessor.treeState.getRawParameterValue("bypass")->load() > 0.5f;

    juce::Colour plateHighlight, plateShadow, accentColor, screenBgColor, cursorCoreColor;

    if (themeChoice == 0) {
        plateHighlight = juce::Colour(0xff3b202e); plateShadow = juce::Colour(0xff1f0c18);
        accentColor = juce::Colour(0xffff66b2); screenBgColor = juce::Colour(0xff1a0a13);
        cursorCoreColor = accentColor;
    }
    else if (themeChoice == 1) {
        plateHighlight = juce::Colour(0xff18222b); plateShadow = juce::Colour(0xff0b1015);
        accentColor = juce::Colour(0xff00d4ff); screenBgColor = juce::Colour(0xff05101a);
        cursorCoreColor = juce::Colour(0xffff007f);
    }
    else if (themeChoice == 2) {
        plateHighlight = juce::Colour(0xff3d281a); plateShadow = juce::Colour(0xff1a0e05);
        accentColor = juce::Colour(0xffffa500); screenBgColor = juce::Colour(0xff140803);
        cursorCoreColor = accentColor;
    }
    else if (themeChoice == 3) {
        plateHighlight = juce::Colour(0xff1b162b); plateShadow = juce::Colour(0xff0a0714);
        accentColor = juce::Colour(0xff9d4dff); screenBgColor = juce::Colour(0xff06020f);
        cursorCoreColor = accentColor;
    }
    else {
        plateHighlight = juce::Colour(0xff3a2e1d); plateShadow = juce::Colour(0xff171007);
        accentColor = juce::Colour(0xffffcc66); screenBgColor = juce::Colour(0xff120b04);
        cursorCoreColor = accentColor;
    }

    for (auto* knob : { &inGainSlider, &outGainSlider, &revSizeSlider, &widthSlider })
        knob->setColour(juce::Slider::rotarySliderFillColourId, accentColor);

    // AGGIORNAMENTO DINAMICO COLORI NUOVO SLIDER ER
    erLevelSlider.setColour(juce::Slider::trackColourId, accentColor.withAlpha(0.2f));
    erLevelSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::black.withAlpha(0.8f));
    erLevelSlider.setColour(juce::Slider::thumbColourId, accentColor);

    mixSlider.setColour(juce::Slider::trackColourId, accentColor.withAlpha(0.48f));
    mixSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::black.withAlpha(0.55f));
    mixSlider.setColour(juce::Slider::thumbColourId, accentColor);
    mixSlider.setColour(juce::Slider::textBoxTextColourId, ivory);
    mixSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    g.fillAll(juce::Colours::black);
    const auto& woodImg = backgrounds[static_cast<size_t>(themeChoice)];
    if (woodImg.isValid()) g.drawImage(woodImg, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination, false);
    else { g.setColour(juce::Colour(0xff111111)); g.fillRect(getLocalBounds().toFloat()); }

    g.addTransform(layoutTransform());

    juce::Rectangle<float> mainPlate(20.0f, 20.0f, 640.0f, 480.0f);
    juce::Rectangle<float> leftPanel(40.0f, 75.0f, 50.0f, 400.0f);
    juce::Rectangle<float> screenArea(105.0f, 75.0f, 465.0f, 400.0f);
    juce::Rectangle<float> rightPanel(585.0f, 75.0f, 60.0f, 400.0f);

    // 2. PLANCIA METALLICA
    juce::Colour shadowColor = juce::Colours::black.interpolatedWith(plateShadow, 0.4f);
    for (float i = 0.0f; i < 18.0f; i += 1.5f) {
        g.setColour(shadowColor.withAlpha(0.18f - (i * 0.008f)));
        g.fillRoundedRectangle(mainPlate.translated(i * 0.2f, i * 0.6f).expanded(i * 0.15f), 12.0f + (i * 0.4f));
    }

    juce::ColourGradient plateGrad(plateHighlight, mainPlate.getX(), mainPlate.getY(), plateShadow, mainPlate.getX(), mainPlate.getBottom(), false);
    g.setGradientFill(plateGrad);
    g.fillRoundedRectangle(mainPlate, 12.0f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(mainPlate, 12.0f, 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(mainPlate.reduced(1.0f), 11.0f, 1.5f);

    const auto& screwImg = screwArtwork;
    float screwSize = 16.0f;
    float margin = 10.0f;

    auto drawScrew = [&](float x, float y, float angle) {
        if (screwImg.isValid()) {
            juce::AffineTransform t = juce::AffineTransform::rotation(angle, x + screwSize / 2, y + screwSize / 2);
            g.addTransform(t);
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.fillEllipse(x + 1.0f, y + 1.0f, screwSize, screwSize);
            g.drawImage(screwImg, x, y, screwSize, screwSize, 0, 0, screwImg.getWidth(), screwImg.getHeight(), false);
            g.addTransform(t.inverted());
        }
        else {
            g.setColour(juce::Colours::black.withAlpha(0.9f));
            g.fillEllipse(x, y, screwSize, screwSize);
            g.setColour(juce::Colour(0xff222222));
            g.fillEllipse(x + 1, y + 1, screwSize - 2, screwSize - 2);
        }
        };

    drawScrew(mainPlate.getX() + margin, mainPlate.getY() + margin, 0.3f);
    drawScrew(mainPlate.getRight() - margin - screwSize, mainPlate.getY() + margin, 1.1f);
    drawScrew(mainPlate.getX() + margin, mainPlate.getBottom() - margin - screwSize, -0.6f);
    drawScrew(mainPlate.getRight() - margin - screwSize, mainPlate.getBottom() - margin - screwSize, 0.8f);

    const juce::Rectangle<float> headerRail(38.0f, 30.0f, 576.0f, 34.0f);
    g.setColour(plateShadow.withAlpha(0.55f));
    g.fillRoundedRectangle(headerRail, 5.0f);
    for (const auto& bounds : { juce::Rectangle<float>(185.0f, 35.0f, 152.0f, 24.0f),
                                juce::Rectangle<float>(345.0f, 35.0f, 76.0f, 24.0f),
                                juce::Rectangle<float>(429.0f, 35.0f, 118.0f, 24.0f),
                                juce::Rectangle<float>(555.0f, 35.0f, 52.0f, 24.0f) })
        drawFaceplate(g, bounds);

    drawCat(g, { 40.0f, 30.0f, 32.0f, 34.0f }, {});
    g.setColour(ivory);
    g.setFont(pixel(16.0f));
    g.drawFittedText("SPACE PANNER", { 76, 29, 103, 24 }, juce::Justification::centredLeft, 1);
    g.setFont(pixel(9.5f));
    g.drawFittedText("ALPHA", { 76, 49, 103, 12 }, juce::Justification::centredLeft, 1);
    g.setFont(pixel(11.0f));
    g.drawText("MIX", juce::Rectangle<int>{ 230, 477, 33, 21 }, juce::Justification::centredLeft);

    // PANNELLO METER SINISTRO
    float pL = audioProcessor.peakL.load();
    float pR = audioProcessor.peakR.load();
    juce::Rectangle<float> meterL(leftPanel.getX() + 10.0f, leftPanel.getY() + 20.0f, 10.0f, 340.0f);
    juce::Rectangle<float> meterR(leftPanel.getX() + 30.0f, leftPanel.getY() + 20.0f, 10.0f, 340.0f);

    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRoundedRectangle(meterL.expanded(2.0f), 3.0f); g.fillRoundedRectangle(meterR.expanded(2.0f), 3.0f);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(meterL, 2.0f); g.fillRoundedRectangle(meterR, 2.0f);

    g.setColour(pL > 0.99f ? juce::Colours::red : accentColor);
    g.fillRoundedRectangle(meterL.withTrimmedTop(meterL.getHeight() * (1.0f - juce::jlimit(0.0f, 1.0f, pL))), 2.0f);
    g.setColour(pR > 0.99f ? juce::Colours::red : accentColor);
    g.fillRoundedRectangle(meterR.withTrimmedTop(meterR.getHeight() * (1.0f - juce::jlimit(0.0f, 1.0f, pR))), 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(pixel(11.0f));
    g.drawText("L", meterL.getX() - 10, meterL.getBottom() + 10, 30, 15, juce::Justification::centred, false);
    g.drawText("R", meterR.getX() - 10, meterR.getBottom() + 10, 30, 15, juce::Justification::centred, false);

    // 5. PANNELLO MANOPOLE DESTRO E TASTI
    g.setColour(ivory.withAlpha(0.9f));
    g.setFont(pixel(11.0f));
    g.drawText("IN GAIN", rightPanel.getX(), rightPanel.getY() + 15, rightPanel.getWidth(), 15, juce::Justification::centred, false);
    g.drawText("OUT GAIN", rightPanel.getX(), rightPanel.getY() + 100, rightPanel.getWidth(), 15, juce::Justification::centred, false);
    g.drawText("SIZE", rightPanel.getX(), rightPanel.getY() + 185, rightPanel.getWidth(), 15, juce::Justification::centred, false);
    g.drawText("WIDTH", rightPanel.getX(), rightPanel.getY() + 270, rightPanel.getWidth(), 15, juce::Justification::centred, false);

    bool isSendMode = audioProcessor.treeState.getRawParameterValue("sendMode")->load() > 0.5f;

    // Ricalcolo esatto per incolonnali perfettamente (nessun Overlap)
    juce::Rectangle<float> advBtnArea(rightPanel.getX() + 5.0f, rightPanel.getY() + 340.0f, 50.0f, 22.0f);
    juce::Rectangle<float> sendBtnArea(rightPanel.getX() + 5.0f, rightPanel.getY() + 368.0f, 50.0f, 26.0f);

    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRoundedRectangle(advBtnArea.expanded(2.0f), 5.0f);
    g.setColour(isAdvancedPanelOpen ? accentColor.darker(0.3f) : juce::Colour(0xff151515));
    g.fillRoundedRectangle(advBtnArea, 4.0f);
    g.setColour(isAdvancedPanelOpen ? accentColor : juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(advBtnArea, 4.0f, 1.0f);
    g.setColour(isAdvancedPanelOpen ? juce::Colours::white : juce::Colours::white.withAlpha(0.5f));
    g.setFont(pixel(11.0f));
    g.drawText("ADV", advBtnArea, juce::Justification::centred, false);

    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRoundedRectangle(sendBtnArea.expanded(2.0f), 5.0f);
    g.setColour(isSendMode ? accentColor.darker(0.3f) : juce::Colour(0xff222222));
    g.fillRoundedRectangle(sendBtnArea, 4.0f);
    g.setColour(isSendMode ? accentColor : juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(sendBtnArea, 4.0f, 2.0f);
    g.setColour(isSendMode ? juce::Colours::white : juce::Colours::white.withAlpha(0.4f));
    g.setFont(pixel(11.0f));
    g.drawText("SEND", sendBtnArea, juce::Justification::centred, false);

    // 6. SCHERMO CENTRALE
    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.fillRoundedRectangle(screenArea.expanded(4.0f), 8.0f);
    g.setColour(screenBgColor);
    g.fillRoundedRectangle(screenArea, 6.0f);

    g.saveState();
    g.reduceClipRegion(screenArea.toNearestInt());

    const auto& themeImg = centralArtwork[static_cast<size_t>(themeChoice)];
    if (themeImg.isValid())
    {
        const auto artBounds = screenArea.reduced(22.0f, 24.0f);
        if (themeChoice == 0 || themeChoice == 1)
        {
            g.setColour(accentColor.withAlpha(0.38f));
            g.drawImage(themeImg, artBounds, juce::RectanglePlacement::centred, true);
        }
        else
        {
            g.setOpacity(0.65f);
            g.drawImage(themeImg, artBounds, juce::RectanglePlacement::centred, false);
            g.setOpacity(1.0f);
        }
    }

    {
        juce::Rectangle<float> scopeBounds = screenArea.reduced(20.0f);
        juce::Path lissajous;
        for (size_t i = 0; i < scopeHistory.size(); ++i) {
            const auto [l, r] = scopeHistory[i];
            float x = juce::jlimit(scopeBounds.getX(), scopeBounds.getRight(), juce::jmap(l * 1.5f, -1.0f, 1.0f, scopeBounds.getX(), scopeBounds.getRight()));
            float y = juce::jlimit(scopeBounds.getY(), scopeBounds.getBottom(), juce::jmap(r * 1.5f, -1.0f, 1.0f, scopeBounds.getBottom(), scopeBounds.getY()));
            if (i == 0) lissajous.startNewSubPath(x, y); else lissajous.lineTo(x, y);
        }
        g.setColour(accentColor.withAlpha(isBypassed ? 0.26f : 0.57f));
        g.strokePath(lissajous, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved));

        if (!isBypassed)
        {
            const auto curX = audioProcessor.treeState.getRawParameterValue("posX")->load();
            float curY = audioProcessor.treeState.getRawParameterValue("posY")->load();
            float curZ = audioProcessor.treeState.getRawParameterValue("posZ")->load();
            float baseSize = juce::jmap(curZ, 0.0f, 1.0f, 45.0f, 12.0f);

            float drawX = juce::jmap(curX, -1.0f, 1.0f, scopeBounds.getX(), scopeBounds.getRight());
            float drawY = juce::jmap(curY, 1.0f, -1.0f, scopeBounds.getY(), scopeBounds.getBottom());

            g.setColour(accentColor.withAlpha(0.2f)); g.fillEllipse(drawX - baseSize, drawY - baseSize, baseSize * 2.0f, baseSize * 2.0f);
            g.setColour(accentColor.withAlpha(0.6f)); g.fillEllipse(drawX - (baseSize * 0.7f), drawY - (baseSize * 0.7f), baseSize * 1.4f, baseSize * 1.4f);
            g.setColour(cursorCoreColor.withMultipliedBrightness(1.0f - (curZ * 0.3f))); g.fillEllipse(drawX - (baseSize * 0.4f), drawY - (baseSize * 0.4f), baseSize * 0.8f, baseSize * 0.8f);
        }
        else
        {
            g.setColour(juce::Colours::red.withAlpha(0.8f));
            g.setFont(pixel(24.0f));
            g.drawText("BYPASSED", screenArea.withHeight(36.0f), juce::Justification::centred, false);
        }
    }
    g.restoreState();

    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.06f), screenArea.getX(), screenArea.getY(), juce::Colours::transparentWhite, screenArea.getRight(), screenArea.getBottom(), false);
    g.setGradientFill(glassGrad); g.fillRoundedRectangle(screenArea, 6.0f);

    auto styleUI = [&](juce::Component& c) {
        c.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        c.setColour(juce::ComboBox::textColourId, accentColor);
        c.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        };
    styleUI(themeSelector); styleUI(presetSelector);
    presetSelector.setColour(juce::ComboBox::textColourId, ivory);

    savePresetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    savePresetButton.setColour(juce::TextButton::textColourOffId, ivory);
    bypassButton.setColour(juce::ToggleButton::textColourId, ivory);
    bypassButton.setColour(juce::ToggleButton::tickColourId, accentColor);

    if (isAdvancedPanelOpen) {
        juce::Rectangle<float> advPlate(670.0f, 20.0f, 330.0f, 480.0f);

        juce::ColourGradient advGrad(plateHighlight, advPlate.getX(), advPlate.getY(), plateShadow, advPlate.getX(), advPlate.getBottom(), false);
        g.setGradientFill(advGrad);
        g.fillRoundedRectangle(advPlate, 12.0f);

        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(advPlate, 12.0f, 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(advPlate.reduced(1.0f), 11.0f, 1.5f);

        drawScrew(advPlate.getRight() - margin - screwSize, advPlate.getY() + margin, 0.5f);
        drawScrew(advPlate.getRight() - margin - screwSize, advPlate.getBottom() - margin - screwSize, -0.2f);

        juce::Rectangle<float> advScreen(695.0f, 140.0f, 280.0f, 280.0f);
        g.setColour(juce::Colours::black.withAlpha(0.95f));
        g.fillRoundedRectangle(advScreen.expanded(4.0f), 8.0f);
        g.setColour(screenBgColor.darker(0.3f));
        g.fillRoundedRectangle(advScreen, 6.0f);

        g.setColour(juce::Colours::white.withAlpha(0.05f));
        for (int i = 1; i < 4; ++i) {
            float linePos = advScreen.getWidth() * (i / 4.0f);
            g.drawLine(advScreen.getX() + linePos, advScreen.getY(), advScreen.getX() + linePos, advScreen.getBottom(), 1.0f);
            g.drawLine(advScreen.getX(), advScreen.getY() + linePos, advScreen.getRight(), advScreen.getY() + linePos, 1.0f);
        }

        // Recupero coordinate per disegnare il perimetro unito
        float nx1 = audioProcessor.treeState.getRawParameterValue("node1X")->load();
        float ny1 = audioProcessor.treeState.getRawParameterValue("node1Y")->load();
        float nx2 = audioProcessor.treeState.getRawParameterValue("node2X")->load();
        float ny2 = audioProcessor.treeState.getRawParameterValue("node2Y")->load();
        float nx3 = audioProcessor.treeState.getRawParameterValue("node3X")->load();
        float ny3 = audioProcessor.treeState.getRawParameterValue("node3Y")->load();
        float nx4 = audioProcessor.treeState.getRawParameterValue("node4X")->load();
        float ny4 = audioProcessor.treeState.getRawParameterValue("node4Y")->load();

        float x1 = advScreen.getX() + (nx1 * advScreen.getWidth());
        float y1 = advScreen.getY() + (ny1 * advScreen.getHeight());
        float x2 = advScreen.getX() + (nx2 * advScreen.getWidth());
        float y2 = advScreen.getY() + (ny2 * advScreen.getHeight());
        float x3 = advScreen.getX() + (nx3 * advScreen.getWidth());
        float y3 = advScreen.getY() + (ny3 * advScreen.getHeight());
        float x4 = advScreen.getX() + (nx4 * advScreen.getWidth());
        float y4 = advScreen.getY() + (ny4 * advScreen.getHeight());

        // DISEGNO DELLA STANZA (Ordine 1 -> 2 -> 4 -> 3 -> 1 per formare un perimetro)
        juce::Path roomShape;
        roomShape.startNewSubPath(x1, y1);
        roomShape.lineTo(x2, y2);
        roomShape.lineTo(x4, y4);
        roomShape.lineTo(x3, y3);
        roomShape.closeSubPath();

        // Area interna colorata in trasparenza
        g.setColour(accentColor.withAlpha(0.12f));
        g.fillPath(roomShape);

        // Bordi della stanza
        g.setColour(accentColor.withAlpha(0.6f));
        g.strokePath(roomShape, juce::PathStrokeType(1.5f));

        // Disegno dei Punti Mobili (Pallini) sopra la forma
        std::array<float, 4> nX = { nx1, nx2, nx3, nx4 };
        std::array<float, 4> nY = { ny1, ny2, ny3, ny4 };
        for (int i = 0; i < 4; ++i) {
            float drawX = advScreen.getX() + (nX[i] * advScreen.getWidth());
            float drawY = advScreen.getY() + (nY[i] * advScreen.getHeight());

            if (draggedNodeIndex == (i + 1)) {
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.fillEllipse(drawX - 12.0f, drawY - 12.0f, 24.0f, 24.0f);
            }
            else {
                g.setColour(accentColor.withAlpha(0.3f));
                g.fillEllipse(drawX - 10.0f, drawY - 10.0f, 20.0f, 20.0f);
            }

            g.setColour(accentColor);
            g.fillEllipse(drawX - 4.0f, drawY - 4.0f, 8.0f, 8.0f);

            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(pixel(12.0f));
            g.drawText(juce::String(i + 1), drawX + 8, drawY - 10, 20, 20, juce::Justification::centredLeft, false);
        }

        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.06f), advScreen.getX(), advScreen.getY(), juce::Colours::transparentWhite, advScreen.getRight(), advScreen.getBottom(), false));
        g.fillRoundedRectangle(advScreen, 6.0f);

        // Disegno Label Slider Sotto il riquadro
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(pixel(11.0f));
        g.drawText("EARLY REFLECTIONS MIX", advScreen.getX(), advScreen.getBottom() + 15, advScreen.getWidth(), 15, juce::Justification::centred, false);

        // Icone Procedurali Superiori
        juce::Rectangle<float> iconArea(advScreen.getX() + 90.0f, 40.0f, 100.0f, 80.0f);
        g.setColour(accentColor.withAlpha(0.9f));
        float cx = iconArea.getCentreX();
        float cy = iconArea.getCentreY();

        if (themeChoice == 0) {
            g.fillEllipse(cx - 20.0f, cy - 15.0f, 40.0f, 30.0f);
            juce::Path ears;
            ears.addTriangle(cx - 18.0f, cy - 5.0f, cx - 10.0f, cy - 25.0f, cx - 2.0f, cy - 12.0f);
            ears.addTriangle(cx + 18.0f, cy - 5.0f, cx + 10.0f, cy - 25.0f, cx + 2.0f, cy - 12.0f);
            g.fillPath(ears);
            g.setColour(screenBgColor);
            g.drawLine(cx - 10.0f, cy, cx - 6.0f, cy + 3.0f, 2.0f);
            g.drawLine(cx - 6.0f, cy + 3.0f, cx - 2.0f, cy, 2.0f);
            g.drawLine(cx + 10.0f, cy, cx + 6.0f, cy + 3.0f, 2.0f);
            g.drawLine(cx + 6.0f, cy + 3.0f, cx + 2.0f, cy, 2.0f);
        }
        else if (themeChoice == 1) {
            g.drawEllipse(cx - 30.0f, cy - 30.0f, 60.0f, 60.0f, 4.0f);
            g.fillEllipse(cx - 20.0f, cy - 20.0f, 40.0f, 40.0f);
            g.setColour(screenBgColor);
            g.fillEllipse(cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        }
        else if (themeChoice == 2) {
            g.fillEllipse(cx - 25.0f, cy - 20.0f, 50.0f, 50.0f);
            g.setColour(screenBgColor);
            g.fillRect(cx - 30.0f, cy + 5.0f, 60.0f, 30.0f);
            g.setColour(accentColor);
            g.drawLine(cx - 30.0f, cy + 10.0f, cx + 30.0f, cy + 10.0f, 2.0f);
            g.drawLine(cx - 20.0f, cy + 16.0f, cx + 20.0f, cy + 16.0f, 2.0f);
            g.drawLine(cx - 10.0f, cy + 22.0f, cx + 10.0f, cy + 22.0f, 2.0f);
        }
        else if (themeChoice == 3) {
            g.fillEllipse(cx - 18.0f, cy - 18.0f, 36.0f, 36.0f);
            juce::Path ring;
            ring.addEllipse(cx - 35.0f, cy - 10.0f, 70.0f, 20.0f);
            g.addTransform(juce::AffineTransform::rotation(-0.3f, cx, cy));
            g.strokePath(ring, juce::PathStrokeType(3.0f));
            g.addTransform(juce::AffineTransform::rotation(0.3f, cx, cy));
        }
        else if (themeChoice == 4) {
            g.fillRect(cx - 4.0f, cy + 5.0f, 8.0f, 20.0f);
            juce::Path leaves;
            leaves.addTriangle(cx, cy - 30.0f, cx - 20.0f, cy - 5.0f, cx + 20.0f, cy - 5.0f);
            leaves.addTriangle(cx, cy - 15.0f, cx - 25.0f, cy + 10.0f, cx + 25.0f, cy + 10.0f);
            g.fillPath(leaves);
        }
    }
}

juce::AffineTransform GettoneSpacePannerEditor::layoutTransform() const
{
    const auto width = isAdvancedPanelOpen ? 1020.0f : 680.0f;
    const auto scale = juce::jmin((float) getWidth() / width, (float) getHeight() / 520.0f);
    return juce::AffineTransform::scale(scale).translated(((float) getWidth() - width * scale) * 0.5f,
                                                        ((float) getHeight() - 520.0f * scale) * 0.5f);
}

void GettoneSpacePannerEditor::resized()
{
    const auto transform = layoutTransform();
    auto place = [&](juce::Component& control, juce::Rectangle<float> bounds) {
        control.setBounds(bounds.transformedBy(transform).toNearestInt());
    };

    place(themeSelector, { 185, 35, 152, 24 });
    place(bypassButton, { 345, 35, 76, 24 });
    place(roomModeSelector, { 105, 477, 101, 21 });
    place(mixSlider, { 264, 478, 299, 19 });
    place(presetSelector, { 429, 35, 118, 24 });
    place(savePresetButton, { 555, 35, 52, 24 });

    place(inGainSlider, { 590, 105, 50, 50 });
    place(outGainSlider, { 590, 190, 50, 50 });
    place(revSizeSlider, { 590, 275, 50, 50 });
    place(widthSlider, { 590, 360, 50, 50 });
    place(advancedButton, { 590, 415, 50, 22 });
    place(sendModeButton, { 590, 443, 50, 26 });
    place(erLevelSlider, { 705, 455, 260, 20 });
    erLevelSlider.setVisible(isAdvancedPanelOpen);
}

void GettoneSpacePannerEditor::beginDragGesture(const juce::String& xId, const juce::String& yId)
{
    dragParameters = { audioProcessor.treeState.getParameter(xId), audioProcessor.treeState.getParameter(yId) };
    for (auto* parameter : dragParameters)
        parameter->beginChangeGesture();
}

void GettoneSpacePannerEditor::endDragGesture()
{
    for (auto*& parameter : dragParameters)
        if (parameter != nullptr)
        {
            parameter->endChangeGesture();
            parameter = nullptr;
        }
    draggedNodeIndex = -1;
}

void GettoneSpacePannerEditor::mouseDown(const juce::MouseEvent& e)
{
    endDragGesture();
    if (audioProcessor.treeState.getRawParameterValue("bypass")->load() > 0.5f)
        return;

    const auto pos = e.position.transformedBy(layoutTransform().inverted());
    const juce::Rectangle<float> advScreen(695.0f, 140.0f, 280.0f, 280.0f);
    if (isAdvancedPanelOpen && advScreen.contains(pos))
    {
        const float mouseX = (pos.x - advScreen.getX()) / advScreen.getWidth();
        const float mouseY = (pos.y - advScreen.getY()) / advScreen.getHeight();
        float minDistance = 0.15f;
        for (int i = 1; i <= 4; ++i)
        {
            const auto node = "node" + juce::String(i);
            const float nx = audioProcessor.treeState.getRawParameterValue(node + "X")->load();
            const float ny = audioProcessor.treeState.getRawParameterValue(node + "Y")->load();
            const float distance = std::hypot(nx - mouseX, ny - mouseY);
            if (distance < minDistance)
            {
                minDistance = distance;
                draggedNodeIndex = i;
            }
        }
        if (draggedNodeIndex != -1)
        {
            const auto node = "node" + juce::String(draggedNodeIndex);
            beginDragGesture(node + "X", node + "Y");
        }
        return;
    }

    const juce::Rectangle<float> pad(105.0f, 75.0f, 465.0f, 400.0f);
    if (pad.contains(pos))
    {
        beginDragGesture("posX", "posY");
        updateParametersFromMouse(pos);
    }
}

void GettoneSpacePannerEditor::updateParametersFromMouse(juce::Point<float> pos)
{
    if (dragParameters[0] == nullptr || audioProcessor.treeState.getRawParameterValue("bypass")->load() > 0.5f)
        return;

    const bool isNode = draggedNodeIndex != -1;
    const auto bounds = isNode ? juce::Rectangle<float>(695.0f, 140.0f, 280.0f, 280.0f)
                               : juce::Rectangle<float>(105.0f, 75.0f, 465.0f, 400.0f).reduced(20.0f);
    const float x = juce::jlimit(0.0f, 1.0f, (pos.x - bounds.getX()) / bounds.getWidth());
    const float y = juce::jlimit(0.0f, 1.0f, (pos.y - bounds.getY()) / bounds.getHeight());
    dragParameters[0]->setValueNotifyingHost(x);
    dragParameters[1]->setValueNotifyingHost(isNode ? y : 1.0f - y);
}

void GettoneSpacePannerEditor::mouseDrag(const juce::MouseEvent& e)
{
    updateParametersFromMouse(e.position.transformedBy(layoutTransform().inverted()));
}

void GettoneSpacePannerEditor::mouseUp(const juce::MouseEvent&)
{
    endDragGesture();
}

void GettoneSpacePannerEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto pos = e.position.transformedBy(layoutTransform().inverted());
    const juce::Rectangle<float> pad(105.0f, 75.0f, 465.0f, 400.0f);
    if (!pad.contains(pos) || audioProcessor.treeState.getRawParameterValue("bypass")->load() > 0.5f)
        return;

    auto& depth = *audioProcessor.treeState.getParameter("posZ");
    const float current = depth.getValue();
    const float next = juce::jlimit(0.0f, 1.0f, current - wheel.deltaY * 0.15f);
    if (next != current)
    {
        depth.beginChangeGesture();
        depth.setValueNotifyingHost(next);
        depth.endChangeGesture();
    }
}
