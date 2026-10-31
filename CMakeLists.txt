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

# KID's on-device speech recognition (push-to-talk commands only): whisper.cpp, MIT licence, pinned to release v1.9.5. The model file is NOT bundled or
# downloaded at build time: KID downloads it when the artist asks, and checks its SHA-1 before loading it. -DGLITCHKID_SPEECH=OFF builds KID without speech.
option(GLITCHKID_SPEECH "Build KID's on-device speech recognition (whisper.cpp)" ON)
if(GLITCHKID_SPEECH)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)   # a portable binary, not tuned to the build machine (x86-64 builds need AVX2, 2013+ CPUs)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)   # no OpenMP runtime inside a host
    set(GGML_METAL OFF CACHE BOOL "" FORCE)    # CPU (+ Accelerate on macOS): no GPU resources to manage inside a host
    set(GGML_BLAS OFF CACHE BOOL "" FORCE)     # no BLAS backend: its Accelerate calls need macOS 13.3, the plug-in supports 12.0 (the CPU backend keeps vDSP)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)    # linked into the plug-in bundle
    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG        d1be6fde11ac6e0407606b4e42fe72d34add8037) # = tag v1.9.5
    FetchContent_MakeAvailable(whisper)
endif()

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
    Source/Analysis/ReferenceAnalyzer.cpp
    Source/AutoMode/AutoMix.cpp
    Source/AutoMode/SourceRelationship.cpp
    Source/AutoMode/MixEngine.cpp
    Source/AutoMode/BeatLock.cpp
    Source/AutoMode/ReferenceMatch.cpp
    Source/Macros/MacroEngine.cpp
    Source/Kid/KidDirector.cpp
    Source/Kid/KidSpeech.cpp
    Source/UI/PlaygroundWidgets.cpp
    Source/UI/LabKit.cpp
    Source/UI/GlitchRelic.cpp
    Source/UI/ReferencePanel.cpp
    Source/UI/KidPanel.cpp
    Source/UI/LabPage.cpp
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
    JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_OPUS=0 # the audio-file formats Reference Match reads and advertises
    JUCE_VST3_CAN_REPLACE_VST2=0
    GLITCHKID_VERSION="${PROJECT_VERSION}")

if(GLITCHKID_SPEECH)
    target_link_libraries(GlitchKid PRIVATE whisper)
    target_compile_definitions(GlitchKid PRIVATE GLITCHKID_SPEECH=1)
endif()
target_link_libraries(GlitchKid
    PRIVATE juce::juce_audio_utils
            juce::juce_dsp
            GlitchKidAssets
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

# Floating-point contraction off. Apple Silicon (and any x86 build with FMA enabled) compilers fuse a*b+c into one instruction that rounds once, and make that choice
# separately for every inlined copy of the same code. That makes (1) parameter values land one rounding step off their grid (JUCE's snap to 0.1 dB computes
# -12 + 0.1f * 120, which fused is 1.8e-7 instead of exactly 0, and a -1 dB ceiling reads -0.99999982), and (2) the same DSP function give results that differ by one
# float step depending on where it was inlined (the plug-in's Formant shifter against the standalone one). The product promises exact neutral defaults and bit-exact
# paths, so the same source must give the same bits on every platform; the cost (separate multiply and add) is negligible for this plug-in.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(GlitchKid PUBLIC -ffp-contract=off)
endif()


# Unit tests for the processor: cmake -S . -B build -DGLITCHKID_BUILD_TESTS=ON && ctest --test-dir build
option(GLITCHKID_BUILD_TESTS "Build the GlitchKid unit tests" OFF)
if(GLITCHKID_BUILD_TESTS)
    enable_testing()
    # which source the test executable was built from: a header regenerated on EVERY build (see cmake/BuildStamp.cmake), printed at the start of a test run
    set(GLITCHKID_STAMP_HEADER "${CMAKE_BINARY_DIR}/generated/GlitchKidBuildStamp.h")
    execute_process(COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${GLITCHKID_STAMP_HEADER} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildStamp.cmake)
    add_custom_target(GlitchKidBuildStamp
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${GLITCHKID_STAMP_HEADER} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildStamp.cmake
        BYPRODUCTS ${GLITCHKID_STAMP_HEADER}
        COMMENT "Recording which source this build is made from")
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
        Tests/ShellTests.cpp
        Tests/VisualTests.cpp
        Tests/ReferenceMatchTests.cpp
        Tests/FloatDeterminismTests.cpp
        Tests/AutoModeTests.cpp
        Tests/MacroTests.cpp
        Tests/DoublingHarmonyTests.cpp
        Tests/KidTests.cpp
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
        Source/Analysis/ReferenceAnalyzer.cpp
        Source/AutoMode/ReferenceMatch.cpp
        Source/AutoMode/AutoMix.cpp
    Source/AutoMode/SourceRelationship.cpp
    Source/AutoMode/MixEngine.cpp
    Source/AutoMode/BeatLock.cpp
        Source/Macros/MacroEngine.cpp
        Source/Kid/KidDirector.cpp
        Source/Kid/KidSpeech.cpp
        Source/UI/PlaygroundWidgets.cpp
        Source/UI/LabKit.cpp
        Source/UI/GlitchRelic.cpp
        Source/UI/ReferencePanel.cpp
        Source/UI/KidPanel.cpp
        Source/UI/LabPage.cpp
        Source/Analysis/DisplaySpectrum.cpp
        Source/UI/EffectModel.cpp
        Source/AlterEgos.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(GlitchKidTests PUBLIC
        GLITCHKID_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_OPUS=0
        GLITCHKID_VERSION="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1) # tests only: lets the editor smoke test run its timer
    if(GLITCHKID_SPEECH)
        target_link_libraries(GlitchKidTests PRIVATE whisper)
        target_compile_definitions(GlitchKidTests PRIVATE GLITCHKID_SPEECH=1)
    endif()
    target_link_libraries(GlitchKidTests
        PRIVATE juce::juce_audio_utils
                juce::juce_dsp
                GlitchKidAssets
        PUBLIC  juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(GlitchKidTests PRIVATE -ffp-contract=off) # the same numeric rules as the plug-in (see above)
    endif()
    add_dependencies(GlitchKidTests GlitchKidBuildStamp)
    target_include_directories(GlitchKidTests PRIVATE ${CMAKE_BINARY_DIR}/generated)
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
