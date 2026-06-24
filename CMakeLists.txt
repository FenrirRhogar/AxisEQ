cmake_minimum_required(VERSION 3.22)

project(ProEQ VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE framework root is two directories above this project
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

# ──────────────────────────────────────────────────────────────────────────────
# Plugin target
# ──────────────────────────────────────────────────────────────────────────────
juce_add_plugin(ProEQ
    COMPANY_NAME             "ProEQ Audio"
    BUNDLE_ID                "com.proeqaudio.ProEQ"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Prqa
    PLUGIN_CODE              Preq
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "ProEQ"
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

juce_generate_juce_header(ProEQ)

# ──────────────────────────────────────────────────────────────────────────────
# Source files  (add new .cpp files here as phases are implemented)
# ──────────────────────────────────────────────────────────────────────────────
target_sources(ProEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Utilities/ParameterLayout.cpp
    Source/Utilities/PresetManager.cpp
    Source/DSP/FilterProcessor.cpp
    Source/DSP/DynamicEQProcessor.cpp
    Source/GUI/SpectrumAnalyzer.cpp
    Source/GUI/EQCurveComponent.cpp
    Source/GUI/ControlPanelComponent.cpp
)

target_include_directories(ProEQ
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# ──────────────────────────────────────────────────────────────────────────────
# Compile definitions
# ──────────────────────────────────────────────────────────────────────────────
target_compile_definitions(ProEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ──────────────────────────────────────────────────────────────────────────────
# Linked JUCE modules
# ──────────────────────────────────────────────────────────────────────────────
target_link_libraries(ProEQ
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
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
