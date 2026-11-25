cmake_minimum_required(VERSION 3.22)

# Porting macOS per Skeuomorphic Space Panner / gett1.
# Il progetto precedente era JUCE/Projucer; questa e' una build CMake indipendente.
project(SkeuomorphicSpacePanner VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Blocca la versione per evitare che una futura modifica di JUCE rompa la build.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PANNER_FORMATS VST3 AU Standalone)
else()
    # Serve anche per un eventuale controllo locale su Windows.
    set(PANNER_FORMATS VST3 Standalone)
endif()

# Conserva l’identità VST3 e AU della versione macOS 1.0.0.
juce_add_plugin(SpacePanner
    COMPANY_NAME "gett1"
    BUNDLE_ID "com.gett1.skeuomorphicspacepanner"
    PLUGIN_MANUFACTURER_CODE Gett
    PLUGIN_CODE Spnr
    PLUGIN_NAME "Skeuomorphic Space Panner"
    PRODUCT_NAME "Skeuomorphic Space Panner"
    DESCRIPTION "Creative stereo spatial panner with interactive early reflections"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${PANNER_FORMATS}
)

juce_generate_juce_header(SpacePanner)

target_sources(SpacePanner PRIVATE
    ../Source/PluginProcessor.cpp
    ../Source/PluginProcessor.h
    ../Source/PluginEditor.cpp
    ../Source/PluginEditor.h
)

# Tutti gli asset sono dentro la build, SENZA riferimenti al vecchio disco Windows.
juce_add_binary_data(SpacePannerAssets
    SOURCES
        Resources/PixelifySans.ttf
        Resources/anime.png
        Resources/beach.png
        Resources/cosmic.png
        Resources/graffiti.png
        Resources/screw.png
        Resources/tree.png
        Resources/wood_beach.jpg
        Resources/wood_cosmic.jpg
        Resources/wood_garage.jpg
        Resources/wood_kawaii.jpg
        Resources/wood_vintage.jpg
)

target_compile_definitions(SpacePanner PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SpacePanner PRIVATE
    SpacePannerAssets
    juce::juce_audio_utils
    juce::juce_dsp
)

include(../Tests/DspTests.cmake)
panner_add_dsp_tests(SpacePanner)
