cmake_minimum_required(VERSION 3.22)
project(OZmosys VERSION 0.10.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE: use a local copy if present, otherwise download it ----
set(JUCE_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Optional local JUCE checkout")
if(EXISTS "${JUCE_LOCAL_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_LOCAL_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

option(FOURFOLD_BUILD_TESTS "Build the offline render test" OFF)

set(FOURFOLD_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthVoice.cpp
    Source/Presets.cpp
    Source/sampler/Library.cpp
    Source/sampler/SamplerVoice.cpp
    Source/sampler/SamplerEngine.cpp
    Source/gui/SamplerPage.cpp
    Source/gui/MapView.cpp
    Source/gui/PadGrid.cpp
    Source/gui/RollPage.cpp
    Source/gui/EnvelopePage.cpp
    Source/gui/BrowsePage.cpp
    Source/gui/FmPage.cpp
    Source/gui/PatchPage.cpp
    Source/gui/WaveDrawPage.cpp
    Source/gui/ExpansionsPage.cpp
    Source/design/Packs.cpp
    Source/roll/RollEngine.cpp
    Source/design/Sections.cpp
    Source/design/Mutator.cpp
    Source/design/SoundFeatures.cpp
    Source/dsp/Wavetable.cpp)

set(FOURFOLD_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1)

set(FOURFOLD_LIBS
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

juce_add_plugin(OZmosys
    COMPANY_NAME "OZ-mosys"
    PLUGIN_MANUFACTURER_CODE Ozmo
    PLUGIN_CODE Ozms
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "OZ-mosys"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(OZmosys PRIVATE ${FOURFOLD_SOURCES})
target_compile_definitions(OZmosys PUBLIC ${FOURFOLD_DEFS})
target_link_libraries(OZmosys
    PRIVATE ${FOURFOLD_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(FOURFOLD_BUILD_TESTS)
    juce_add_console_app(FourfoldRender PRODUCT_NAME "FourfoldRender")
    target_sources(FourfoldRender PRIVATE ${FOURFOLD_SOURCES} tools/RenderTest.cpp)
    target_compile_definitions(FourfoldRender PRIVATE ${FOURFOLD_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(FourfoldRender PRIVATE ${FOURFOLD_LIBS} juce::juce_recommended_config_flags)
endif()
