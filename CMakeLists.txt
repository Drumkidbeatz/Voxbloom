cmake_minimum_required(VERSION 3.22)

# Universal binary + sane deployment target on macOS (must be set before project()).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(VoxBloom VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VOXBLOOM_COPY_AFTER_BUILD "Copy the built plugin into the system plugin folder" OFF)
option(VOXBLOOM_BUILD_TESTS "Build the headless test runner" ON)
set(VOXBLOOM_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (otherwise JUCE is downloaded)")

if(VOXBLOOM_JUCE_DIR)
    add_subdirectory(${VOXBLOOM_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(VoxBloom
    COMPANY_NAME              "Drumkidbeatz"
    BUNDLE_ID                 "com.drumkidbeatz.voxbloom"
    PLUGIN_MANUFACTURER_CODE  Dkbz
    PLUGIN_CODE               Vxbl
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "VoxBloom"
    DESCRIPTION               "Vocal delay + reverb with ducking"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    VST3_CATEGORIES           Fx Delay Reverb
    AU_MAIN_TYPE              kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD   ${VOXBLOOM_COPY_AFTER_BUILD})

set(VOXBLOOM_SOURCES
    Source/Params.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/VoxLookAndFeel.cpp)

target_sources(VoxBloom PRIVATE ${VOXBLOOM_SOURCES})
target_include_directories(VoxBloom PRIVATE Source)

set(VOXBLOOM_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(VoxBloom PUBLIC ${VOXBLOOM_DEFINITIONS})

target_link_libraries(VoxBloom
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(VOXBLOOM_BUILD_TESTS)
    juce_add_console_app(VoxBloomTests PRODUCT_NAME "VoxBloomTests")
    target_sources(VoxBloomTests PRIVATE Tests/Tests.cpp ${VOXBLOOM_SOURCES})
    target_include_directories(VoxBloomTests PRIVATE Source)
    target_compile_definitions(VoxBloomTests PRIVATE ${VOXBLOOM_DEFINITIONS} VOXBLOOM_TESTS=1)
    target_link_libraries(VoxBloomTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
