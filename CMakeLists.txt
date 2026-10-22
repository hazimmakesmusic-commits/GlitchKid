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
# JUCE is downloaded at configure time (not stored in this repo), pinned to the exact commit of release 9.0.4.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        3fb5cbc0340758596dac39f7c8de47d562b4896e  # = tag 9.0.4
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

# Bundled fonts (Inter, SIL Open Font License; see Assets/Fonts/OFL.txt): compiled in so the interface looks the same on every machine.
juce_add_binary_data(GlitchKidAssets SOURCES
    Assets/Fonts/Inter-Regular.otf
    Assets/Fonts/Inter-Bold.otf
    Assets/Art/glitch_relic.png) # the Glitch Relic artwork (supplied by the project owner; see Assets/Art/README.md)

target_sources(GlitchKid PRIVATE
    Source/DSP/ParametricEq.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/Saturation.cpp
    Source/DSP/AirLab.cpp
    Source/DSP/CleanLab.cpp
    Source/DSP/OutputLimiter.cpp
    Source/DSP/GlitchStutter.cpp
    Source/DSP/LiquidEcho.cpp
    Source/DSP/Dimension.cpp
    Source/DSP/VocalMutation.cpp
    Source/DSP/VocalDoubling/VocalDoubler.cpp
    Source/DSP/Harmony/HarmonyVoice.cpp
    Source/Presets/SoundState.cpp
    Source/Presets/PresetStore.cpp
    Source/Presets/PresetController.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Analysis/VocalAnalyzer.cpp
    Source/AutoMode/AutoMix.cpp
    Source/AutoMode/SourceRelationship.cpp
    Source/AutoMode/MixEngine.cpp
    Source/AutoMode/BeatLock.cpp
    Source/Macros/MacroEngine.cpp
    Source/UI/PlaygroundWidgets.cpp
    Source/UI/LabKit.cpp
    Source/UI/GlitchRelic.cpp
    Source/Analysis/DisplaySpectrum.cpp
    Source/UI/EffectModel.cpp
    Source/DSP/PitchCorrection/YinPitchDetector.cpp
    Source/DSP/PitchCorrection/ScaleQuantizer.cpp
    Source/DSP/PitchCorrection/PsolaPitchShifter.cpp
    Source/DSP/PitchCorrection/PitchCorrector.cpp
    Source/DSP/FormantShifting/FormantShifter.cpp
    Source/AlterEgos.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GlitchKid PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    GLITCHKID_VERSION="${PROJECT_VERSION}")

target_link_libraries(GlitchKid
    PRIVATE juce::juce_audio_utils
            juce::juce_dsp
            GlitchKidAssets
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

# Unit tests for the processor: cmake -S . -B build -DGLITCHKID_BUILD_TESTS=ON && ctest --test-dir build
option(GLITCHKID_BUILD_TESTS "Build the GlitchKid unit tests" OFF)
if(GLITCHKID_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(GlitchKidTests PRODUCT_NAME "GlitchKidTests")
    target_sources(GlitchKidTests PRIVATE
        Tests/main.cpp
        Tests/CompressorTests.cpp
        Tests/SaturationTests.cpp
        Tests/AirLabTests.cpp
        Tests/CleanLabTests.cpp
        Tests/MixTests.cpp
        Tests/StutterTests.cpp
        Tests/RobustnessTests.cpp
        Tests/EchoTests.cpp
        Tests/DimensionTests.cpp
        Tests/AlterEgoTests.cpp
        Tests/PitchCorrectionTests.cpp
        Tests/PitchTuneTests.cpp
        Tests/FormantShiftTests.cpp
        Tests/FormantTuneTests.cpp
        Tests/MutationTests.cpp
        Tests/PresetTests.cpp
        Tests/FactoryPresetTests.cpp
        Tests/ProfileTests.cpp
        Tests/SourceTests.cpp
        Tests/MixEngineTests.cpp
        Tests/BeatLockTests.cpp
        Tests/LevelUiTests.cpp
        Tests/OutputLimiterTests.cpp
        Tests/UiPerfTests.cpp
        Tests/UiBehaviourTests.cpp
        Tests/VisualTests.cpp
        Tests/AutoModeTests.cpp
        Tests/MacroTests.cpp
        Tests/DoublingHarmonyTests.cpp
        Source/DSP/FormantShifting/FormantShifter.cpp
        Source/DSP/PitchCorrection/YinPitchDetector.cpp
        Source/DSP/PitchCorrection/ScaleQuantizer.cpp
        Source/DSP/PitchCorrection/PsolaPitchShifter.cpp
        Source/DSP/PitchCorrection/PitchCorrector.cpp
        Source/DSP/ParametricEq.cpp
        Source/DSP/Compressor.cpp
        Source/DSP/Saturation.cpp
        Source/DSP/AirLab.cpp
        Source/DSP/CleanLab.cpp
        Source/DSP/OutputLimiter.cpp
        Source/DSP/GlitchStutter.cpp
        Source/DSP/LiquidEcho.cpp
        Source/DSP/Dimension.cpp
        Source/DSP/VocalMutation.cpp
        Source/DSP/VocalDoubling/VocalDoubler.cpp
        Source/DSP/Harmony/HarmonyVoice.cpp
        Source/Presets/SoundState.cpp
        Source/Presets/PresetStore.cpp
        Source/Presets/PresetController.cpp
        Source/Presets/FactoryPresets.cpp
        Source/Analysis/VocalAnalyzer.cpp
        Source/AutoMode/AutoMix.cpp
    Source/AutoMode/SourceRelationship.cpp
    Source/AutoMode/MixEngine.cpp
    Source/AutoMode/BeatLock.cpp
        Source/Macros/MacroEngine.cpp
        Source/UI/PlaygroundWidgets.cpp
        Source/UI/LabKit.cpp
        Source/UI/GlitchRelic.cpp
        Source/Analysis/DisplaySpectrum.cpp
        Source/UI/EffectModel.cpp
        Source/AlterEgos.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(GlitchKidTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        GLITCHKID_VERSION="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1) # tests only: lets the editor smoke test run its timer
    target_link_libraries(GlitchKidTests
        PRIVATE juce::juce_audio_utils
                juce::juce_dsp
                GlitchKidAssets
        PUBLIC  juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
    add_test(NAME GlitchKidProcessor COMMAND GlitchKidTests)
endif()

# Command-line harness for the pitch-correction module (tune a WAV file, print measurements). Not part of the plugin.
#   cmake -S . -B build-lab -DGLITCHKID_BUILD_PITCHLAB=ON && cmake --build build-lab --target PitchLab
option(GLITCHKID_BUILD_PITCHLAB "Build the PitchLab harness" OFF)
if(GLITCHKID_BUILD_PITCHLAB)
    juce_add_console_app(PitchLab PRODUCT_NAME "PitchLab")
    target_sources(PitchLab PRIVATE
        Tools/PitchLab/main.cpp
        Source/DSP/PitchCorrection/YinPitchDetector.cpp
        Source/DSP/PitchCorrection/ScaleQuantizer.cpp
        Source/DSP/PitchCorrection/PsolaPitchShifter.cpp
        Source/DSP/PitchCorrection/PitchCorrector.cpp)
    target_compile_definitions(PitchLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PitchLab
        PRIVATE juce::juce_audio_formats
        PUBLIC  juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
endif()

# Command-line harness for the formant shifter (shift a WAV file, print measurements). Not part of the plugin.
#   cmake -S . -B build-formant -DGLITCHKID_BUILD_FORMANTLAB=ON && cmake --build build-formant --target FormantLab
option(GLITCHKID_BUILD_FORMANTLAB "Build the FormantLab harness" OFF)
if(GLITCHKID_BUILD_FORMANTLAB)
    juce_add_console_app(FormantLab PRODUCT_NAME "FormantLab")
    target_sources(FormantLab PRIVATE
        Tools/FormantLab/main.cpp
        Source/DSP/FormantShifting/FormantShifter.cpp)
    target_compile_definitions(FormantLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FormantLab
        PRIVATE juce::juce_audio_formats
        PUBLIC  juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
endif()

# DoubleLab: command-line listening harness for the Vocal Doubler and the Harmony voice (Stage 4.8).
#   cmake -S . -B build-double -DGLITCHKID_BUILD_DOUBLELAB=ON && cmake --build build-double --target DoubleLab
option(GLITCHKID_BUILD_DOUBLELAB "Build the DoubleLab harness" OFF)
if(GLITCHKID_BUILD_DOUBLELAB)
    juce_add_console_app(DoubleLab PRODUCT_NAME "DoubleLab")
    target_sources(DoubleLab PRIVATE
        Tools/DoubleLab/main.cpp
        Source/DSP/VocalDoubling/VocalDoubler.cpp
        Source/DSP/Harmony/HarmonyVoice.cpp
        Source/DSP/PitchCorrection/YinPitchDetector.cpp
        Source/DSP/PitchCorrection/ScaleQuantizer.cpp
        Source/DSP/PitchCorrection/PsolaPitchShifter.cpp
        Source/DSP/PitchCorrection/PitchCorrector.cpp)
    target_compile_definitions(DoubleLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(DoubleLab
        PRIVATE juce::juce_audio_formats
        PUBLIC  juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
endif()
