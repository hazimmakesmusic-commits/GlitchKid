cmake_minimum_required(VERSION 3.22)

# Must be set before project(). Ignored on non-Apple hosts.
if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple Silicon only (Stage 1A)")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(GlitchKid VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# JUCE is downloaded at configure time (not stored in this repo), pinned to an exact release tag.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GlitchKid
    COMPANY_NAME "GlitchKid"
    PRODUCT_NAME "GlitchKid"
    FORMATS VST3
    PLUGIN_MANUFACTURER_CODE GlKd   # see README: must stay unique and never change after release
    PLUGIN_CODE Gkid
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx)

target_sources(GlitchKid PRIVATE
    Source/DSP/ParametricEq.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GlitchKid PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GlitchKid
    PRIVATE juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

# Unit tests for the processor: cmake -S . -B build -DGLITCHKID_BUILD_TESTS=ON && ctest --test-dir build
option(GLITCHKID_BUILD_TESTS "Build the GlitchKid unit tests" OFF)
if(GLITCHKID_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(GlitchKidTests PRODUCT_NAME "GlitchKidTests")
    target_sources(GlitchKidTests PRIVATE
        Tests/main.cpp
        Source/DSP/ParametricEq.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(GlitchKidTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(GlitchKidTests
        PRIVATE juce::juce_audio_utils
        PUBLIC  juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
    add_test(NAME GlitchKidProcessor COMMAND GlitchKidTests)
endif()
