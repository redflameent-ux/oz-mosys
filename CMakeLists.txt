cmake_minimum_required(VERSION 3.22)
project(VibraVII VERSION 0.13.0)

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
option(FOURFOLD_BUILD_TOOLS "Build local tools (PluginProbe; needs plugins installed to run)" OFF)

set(FOURFOLD_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthVoice.cpp
    Source/Presets.cpp
    Source/sampler/Library.cpp
    Source/sampler/SamplerVoice.cpp
    Source/sampler/SamplerEngine.cpp
    Source/gui/SamplerPage.cpp
    Source/gui/SlicerPanel.cpp
    Source/gui/MapView.cpp
    Source/gui/PadGrid.cpp
    Source/gui/RollPage.cpp
    Source/gui/EnvelopePage.cpp
    Source/gui/BrowsePage.cpp
    Source/gui/FmPage.cpp
    Source/gui/PatchPage.cpp
    Source/gui/WaveDrawPage.cpp
    Source/dsp/Rack.cpp
    Source/gui/FxPage.cpp
    Source/perform/Perform.cpp
    Source/gui/PlayPage.cpp
    Source/gui/MorphPage.cpp
    Source/perform/Morph.cpp
    Source/import/OmniImport.cpp
    Source/import/ArturiaImport.cpp
    Source/import/NIImport.cpp
    Source/import/HiveImport.cpp
    Source/import/DuneImport.cpp
    Source/import/SpireImport.cpp
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

juce_add_plugin(VibraVII
    COMPANY_NAME "TRANCE-6"
    PLUGIN_MANUFACTURER_CODE Trc6
    PLUGIN_CODE Vv7c
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "VIBRA-VII ChromaBed"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Embedded assets (the startup-splash logo). Linked to every target that compiles the editor.
juce_add_binary_data(VibraVIIData SOURCES Resources/logo.png)

target_sources(VibraVII PRIVATE ${FOURFOLD_SOURCES})
target_compile_definitions(VibraVII PUBLIC ${FOURFOLD_DEFS})
target_link_libraries(VibraVII
    PRIVATE ${FOURFOLD_LIBS} VibraVIIData
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(FOURFOLD_BUILD_TESTS)
    juce_add_console_app(FourfoldRender PRODUCT_NAME "FourfoldRender")
    target_sources(FourfoldRender PRIVATE ${FOURFOLD_SOURCES} tools/RenderTest.cpp)
    target_compile_definitions(FourfoldRender PRIVATE ${FOURFOLD_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(FourfoldRender PRIVATE ${FOURFOLD_LIBS} VibraVIIData juce::juce_recommended_config_flags)
endif()

# ---- PluginProbe: headless plugin-oracle that dumps a plugin's parameters by name ----
# Opt-in and local only (it hosts real plugins): cmake -S . -B build -DFOURFOLD_BUILD_TOOLS=ON
if(FOURFOLD_BUILD_TOOLS)
    juce_add_console_app(PluginProbe PRODUCT_NAME "PluginProbe")
    target_sources(PluginProbe PRIVATE tools/PluginProbe.cpp)
    target_compile_definitions(PluginProbe PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PluginProbe PRIVATE
        juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)

    # ---- ImportLib: run any in-plugin importer headlessly over a folder (library conversion) ----
    juce_add_console_app(ImportLib PRODUCT_NAME "ImportLib")
    target_sources(ImportLib PRIVATE ${FOURFOLD_SOURCES} tools/ImportLib.cpp)
    target_compile_definitions(ImportLib PRIVATE ${FOURFOLD_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(ImportLib PRIVATE ${FOURFOLD_LIBS} VibraVIIData juce::juce_recommended_config_flags)

    # ---- PresetForge: generate an original preset library from curated archetypes ----
    juce_add_console_app(PresetForge PRODUCT_NAME "PresetForge")
    target_sources(PresetForge PRIVATE ${FOURFOLD_SOURCES} tools/PresetForge.cpp)
    target_compile_definitions(PresetForge PRIVATE ${FOURFOLD_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(PresetForge PRIVATE ${FOURFOLD_LIBS} VibraVIIData juce::juce_recommended_config_flags)
endif()
