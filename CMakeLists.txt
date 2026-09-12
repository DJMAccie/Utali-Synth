cmake_minimum_required(VERSION 3.22)

project(UTALISYNTH VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate JUCE: prioritize local installation for ultra-fast offline builds, fallback to FetchContent
if(EXISTS "/Users/user/Downloads/JUCE/CMakeLists.txt")
    message(STATUS "Using local JUCE installation at /Users/user/Downloads/JUCE")
    add_subdirectory(/Users/user/Downloads/JUCE juce_build EXCLUDE_FROM_ALL)
elseif(EXISTS "/Users/user/Documents/Utali-Reverbus/NewProject/build/_deps/juce-src/CMakeLists.txt")
    message(STATUS "Using JUCE from Reverbus deps")
    add_subdirectory(/Users/user/Documents/Utali-Reverbus/NewProject/build/_deps/juce-src juce_build EXCLUDE_FROM_ALL)
else()
    message(STATUS "Fetching JUCE 8.0.6 via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# On CI environments (e.g. GitHub Actions), avoid copying plugins to system folders
if(DEFINED ENV{CI})
    set(COPY_PLUGIN_AFTER_BUILD_FLAG FALSE)
else()
    set(COPY_PLUGIN_AFTER_BUILD_FLAG TRUE)
endif()

# AudioUnit is only supported on Apple platforms
if(APPLE)
    set(UTALI_FORMATS VST3 AU Standalone)
else()
    set(UTALI_FORMATS VST3 Standalone)
endif()

juce_add_plugin(UTALISYNTH
    COMPANY_NAME "UTALI"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    
    # Formats
    FORMATS ${UTALI_FORMATS}
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD_FLAG}
    
    # Metadata
    PRODUCT_NAME "UTALISYNTH"
    PLUGIN_NAME "UTALISYNTH"
    PLUGIN_CODE "UtSy"
    PLUGIN_MANUFACTURER_CODE "Utal"
    VERSION "1.0.0"
    DESCRIPTION "Polyphonic Vintage Synthesizer with ADAA Saturation and Chorus"
)

# Binary Assets
juce_add_binary_data(UTALISYNTHAssets
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES
        "Assets/hise_Knob_medium.png"
        "Assets/hise_knob_big.png"
        "Assets/hise_knob_small.png"
        "Assets/met zelfde witte achtergrond 1.png"
        "Assets/met zelfde witte achtergrond 3.png"
        "Assets/vintage_illustrated_panel.png"
)

target_sources(UTALISYNTH PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SynthVoice.cpp
    Source/SynthVoice.h
    Source/ADAAProcessor.h
    Source/GUI/UtaliLookAndFeel.h
)

target_compile_definitions(UTALISYNTH PUBLIC
    JUCE_WEB_BROWSER_NO_WEBVIEW_LOADER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

target_link_libraries(UTALISYNTH PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    UTALISYNTHAssets
)

juce_generate_juce_header(UTALISYNTH)

# Verification Test Console App
juce_add_console_app(UtaliSynthTests
    PRODUCT_NAME "UtaliSynthTests"
)

target_sources(UtaliSynthTests PRIVATE
    Tests/PluginTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/SynthVoice.cpp
    Source/SynthVoice.h
    Source/ADAAProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/GUI/UtaliLookAndFeel.h
)

target_compile_definitions(UtaliSynthTests PUBLIC
    JUCE_WEB_BROWSER_NO_WEBVIEW_LOADER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

target_link_libraries(UtaliSynthTests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    UTALISYNTHAssets
)

juce_generate_juce_header(UtaliSynthTests)
