cmake_minimum_required(VERSION 3.22)

# Keep the MSVC runtime choice consistent across the plug-in and JUCE.
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime" FORCE)

project(ZCRUSH VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE is downloaded as a release archive instead of a shallow Git clone.
# This is more reliable in CI environments and does not depend on Git's
# credential / safe-directory configuration while CMake is configuring.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/7.0.12.zip
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ZCRUSH
    COMPANY_NAME "ZCRUSH Lab"
    PLUGIN_MANUFACTURER_CODE Zcrl
    PLUGIN_CODE Zcr1
    FORMATS VST3
    PRODUCT_NAME "ZCRUSH"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
)

juce_add_binary_data(ZCrushFonts
    SOURCES
        Fonts/LiberationMono-Bold.ttf
        Fonts/LiberationSans-Bold.ttf
)

target_sources(ZCRUSH PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(ZCRUSH PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(ZCRUSH
    PRIVATE
        juce::juce_audio_utils
        ZCrushFonts
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Make compiler diagnostics easier to read in CI.
if (MSVC)
    target_compile_options(ZCRUSH PRIVATE /W4 /permissive-)
else()
    target_compile_options(ZCRUSH PRIVATE -Wall -Wextra -Wpedantic)
endif()
