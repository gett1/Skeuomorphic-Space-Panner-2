cmake_minimum_required(VERSION 3.22)
project(SkeuomorphicSpacePanner VERSION 2.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(GETT1_JUCE_PATH "D:/JUCE" CACHE PATH "JUCE source directory")
if(NOT EXISTS "${GETT1_JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Set GETT1_JUCE_PATH to your JUCE source directory")
endif()

add_subdirectory("${GETT1_JUCE_PATH}" juce)

juce_add_plugin(GettoneSpacePanner
    COMPANY_NAME "gett1"
    BUNDLE_ID "com.gett1.skeuomorphicspacepanner.alpha211"
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE SpAx
    PRODUCT_NAME "Skeuomorphic Space Panner 2.1.1 Alpha"
    PLUGIN_NAME "Skeuomorphic Space Panner 2.1.1 Alpha"
    DESCRIPTION "GettoneSpacePanner_Hardware"
    FORMATS VST3
    VST3_CATEGORIES Fx Spatial Tools
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(GettoneSpacePanner PRIVATE
    ../Source/PluginEditor.cpp
    ../Source/PluginProcessor.cpp
)
if(MSVC)
    target_compile_options(GettoneSpacePanner PRIVATE /utf-8)
endif()
target_compile_definitions(GettoneSpacePanner PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

include(../cmake/Artwork.cmake)
panner_artwork(PannerAssets "immagini Skeuomorphic Panner" Assets)

juce_generate_juce_header(GettoneSpacePanner)
target_link_libraries(GettoneSpacePanner PRIVATE
    PannerAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

include(../Tests/DspTests.cmake)
panner_add_dsp_tests(GettoneSpacePanner)
