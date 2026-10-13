cmake_minimum_required(VERSION 3.22)

project(TUBBY VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(TBY_JUCE_DIR)
    add_subdirectory(${TBY_JUCE_DIR} juce)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

option(TBY_BUILD_TESTS "Build the offline engine test, demo render and UI snapshot" OFF)

set(TUBBY_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(TUBBY_FORMATS VST3 Standalone)
endif()

juce_add_plugin(TUBBY
    COMPANY_NAME                "manners! DSP"
    BUNDLE_ID                   com.mannersdsp.tubby
    PLUGIN_MANUFACTURER_CODE    Mnrs
    PLUGIN_CODE                 Tuby
    PRODUCT_NAME                "TUBBY"
    DESCRIPTION                 "Dub delay with springs"
    FORMATS                     ${TUBBY_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Delay Reverb)

juce_add_binary_data(TUBBY_Assets SOURCES Resources/ShareTechMono-Regular.ttf)

target_sources(TUBBY PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(TUBBY PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TUBBY
    PRIVATE
        TUBBY_Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(TBY_BUILD_TESTS)
    # Engine-only checks: no GUI, runs on the CI machine.
    juce_add_console_app(TUBBY_Test PRODUCT_NAME "TUBBY_Test")
    target_sources(TUBBY_Test PRIVATE Tests/OfflineTest.cpp)
    target_include_directories(TUBBY_Test PRIVATE Source)
    target_compile_definitions(TUBBY_Test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TUBBY_Test PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags)

    # Full plugin: renders a demo WAV and a PNG of the editor.
    juce_add_console_app(TUBBY_Tool PRODUCT_NAME "TUBBY_Tool")
    target_sources(TUBBY_Tool PRIVATE
        Tests/Tool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(TUBBY_Tool PRIVATE Source)
    target_compile_definitions(TUBBY_Tool PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TUBBY_Tool PRIVATE
        TUBBY_Assets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
