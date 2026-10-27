cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

project(AvelEss VERSION 1.0.0 LANGUAGES C CXX)

# Windows: link the C++ runtime statically so no VC++ redistributable is needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise it is downloaded.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(AVEL_FORMATS VST3)
if(APPLE)
    list(APPEND AVEL_FORMATS AU)
endif()

juce_add_plugin(AvelEss
    COMPANY_NAME "Avel"
    BUNDLE_ID "com.avel.ess"
    PLUGIN_MANUFACTURER_CODE Avel
    PLUGIN_CODE AvEs
    FORMATS ${AVEL_FORMATS}
    PRODUCT_NAME "Avel Ess"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(AvelEssFonts SOURCES
    Resources/Fonts/Inter-Regular.otf
    Resources/Fonts/Inter-Medium.otf
    Resources/Fonts/Inter-SemiBold.otf)
set_target_properties(AvelEssFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(AVEL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(AVEL_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    AVEL_VERSION="${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    # Linux is only used for testing; these X11 extras are optional at runtime.
    list(APPEND AVEL_DEFINES JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

set(AVEL_MODULES
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_gui_basics)

target_sources(AvelEss PRIVATE ${AVEL_SOURCES})
target_include_directories(AvelEss PRIVATE Source)
target_compile_definitions(AvelEss PUBLIC ${AVEL_DEFINES})
if(MSVC)
    target_compile_options(AvelEss PRIVATE /utf-8)   # preset names contain "·"
endif()
target_link_libraries(AvelEss
    PRIVATE ${AVEL_MODULES} AvelEssFonts
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline render test (not part of the release build)
option(AVEL_BUILD_TESTS "Build the offline test harness" OFF)
if(AVEL_BUILD_TESTS)
    juce_add_console_app(AvelEssTest PRODUCT_NAME "AvelEssTest")
    target_sources(AvelEssTest PRIVATE Tests/RenderTest.cpp ${AVEL_SOURCES})
    target_include_directories(AvelEssTest PRIVATE Source)
    target_compile_definitions(AvelEssTest PRIVATE ${AVEL_DEFINES} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(AvelEssTest PRIVATE ${AVEL_MODULES} AvelEssFonts juce::juce_recommended_config_flags)
endif()
