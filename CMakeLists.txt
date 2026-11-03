cmake_minimum_required(VERSION 3.22)

project(AxisEQ VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AXISEQ_LTO "Link-time optimisation (slow links; turn off for development)" ON)
set(JUCE_DIR "" CACHE PATH "Local JUCE 8 checkout; fetched from GitHub when empty")

# JUCE framework: a local checkout if given, otherwise the pinned release.
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Plugin target
# ──────────────────────────────────────────────────────────────────────────────
juce_add_plugin(AxisEQ
    COMPANY_NAME             "fenrikos"
    BUNDLE_ID                "com.Axisaudio.AxisEQ"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Axis
    PLUGIN_CODE              AxEq
    FORMATS                  VST3 AU Standalone
    PRODUCT_NAME             "AxisEQ"
    COPY_PLUGIN_AFTER_BUILD  FALSE
    ICON_BIG                 AxisEQ_logo.png
    ICON_SMALL               AxisEQ_logo.png
)

juce_generate_juce_header(AxisEQ)

# ──────────────────────────────────────────────────────────────────────────────
# Source files  (add new .cpp files here as phases are implemented)
# ──────────────────────────────────────────────────────────────────────────────
target_sources(AxisEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Utilities/ParameterLayout.cpp
    Source/Utilities/PresetManager.cpp
    Source/DSP/BandProcessor.cpp
    Source/GUI/SpectrumAnalyzer.cpp
    Source/GUI/EQCurveComponent.cpp
    Source/GUI/ControlPanelComponent.cpp
    Source/GUI/CustomLookAndFeel.cpp
)

target_include_directories(AxisEQ
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# ──────────────────────────────────────────────────────────────────────────────
# Compile definitions
# ──────────────────────────────────────────────────────────────────────────────
target_compile_definitions(AxisEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ──────────────────────────────────────────────────────────────────────────────
# Linked JUCE modules
# ──────────────────────────────────────────────────────────────────────────────
target_link_libraries(AxisEQ
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(AXISEQ_LTO)
    target_link_libraries(AxisEQ PUBLIC juce::juce_recommended_lto_flags)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# DSP unit tests (run build/AxisEQTests_artefacts/<config>/AxisEQTests)
# ──────────────────────────────────────────────────────────────────────────────
option(AXISEQ_TESTS "Build the DSP unit tests" ON)
if(AXISEQ_TESTS)
    juce_add_console_app(AxisEQTests PRODUCT_NAME "AxisEQTests")
    juce_generate_juce_header(AxisEQTests)
    target_sources(AxisEQTests PRIVATE tests/DSPTests.cpp Source/DSP/BandProcessor.cpp)
    target_include_directories(AxisEQTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(AxisEQTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AxisEQTests PRIVATE juce::juce_audio_processors juce::juce_dsp
                                      PUBLIC juce::juce_recommended_config_flags)
endif()
