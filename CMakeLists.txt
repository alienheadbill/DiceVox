cmake_minimum_required(VERSION 3.22)

project(DiceVox VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE is downloaded automatically the first time you configure.
# (To use a local copy instead: -DFETCHCONTENT_SOURCE_DIR_JUCE=path/to/JUCE)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------
include(cmake/AHE.cmake)

# Identity: AHE company code (cmake/AHE.cmake) + DiceVox's own code "Dvox".
# Never change PLUGIN_CODE after release — saved projects would lose the plugin.
ahe_add_plugin(DiceVox
    PRODUCT_NAME            "DiceVox"
    PLUGIN_CODE             Dvox
    FORMATS                 VST3 Standalone
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx)

target_sources(DiceVox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/modules/FxChain.cpp
    Source/modules/FilterModule.cpp
    # Shared AHE Audio UI layer (candidate for the future ahe-core repo)
    Source/ahe/AHEFonts.cpp
    Source/ahe/AHELookAndFeel.cpp
    Source/ahe/AHEBrandMarks.cpp
    Source/ahe/AHEHeader.cpp
    Source/ahe/AHEKnob.cpp
    # DiceVox-specific UI
    Source/ui/RollChamber.cpp)

# Approved AHE Audio brand vectors (design/assets) and OFL fonts (design/fonts),
# compiled into the plug-in. No files are read from disk or the network at runtime.
juce_add_binary_data(AHEBrandData
    HEADER_NAME AHEBrandData.h
    NAMESPACE   AHEBrandData
    SOURCES
        design/assets/ahe-alien-icon.svg
        design/assets/ahe-ufo-icon.svg
        design/assets/ahe-alien-horizontal.svg
        design/fonts/Inter-Regular.ttf
        design/fonts/Inter-SemiBold.ttf
        design/fonts/BarlowCondensed-ExtraBoldItalic.ttf)

target_include_directories(DiceVox PRIVATE Source)

target_compile_definitions(DiceVox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DiceVox
    PRIVATE
        AHEBrandData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline tests (run without a DAW): build target DiceVoxTests
# ---------------------------------------------------------------------------
option(DICEVOX_BUILD_TESTS "Build offline audio tests" ON)
if(DICEVOX_BUILD_TESTS)
    # Plain executable that links the plugin's shared code (inherits its JUCE config).
    add_executable(DiceVoxTests tests/TestMain.cpp)
    target_include_directories(DiceVoxTests PRIVATE
        Source $<TARGET_PROPERTY:DiceVox,INCLUDE_DIRECTORIES>)
    target_compile_definitions(DiceVoxTests PRIVATE
        $<TARGET_PROPERTY:DiceVox,COMPILE_DEFINITIONS>)
    target_link_libraries(DiceVoxTests PRIVATE DiceVox)
endif()

# ---------------------------------------------------------------------------
# UI snapshot tool (design review only, off by default; not used by CI)
# ---------------------------------------------------------------------------
option(DICEVOX_BUILD_SNAPSHOTS "Build the editor-to-PNG snapshot tool" OFF)
if(DICEVOX_BUILD_SNAPSHOTS)
    add_executable(DiceVoxSnapshots tools/snapshots/SnapshotMain.cpp)
    target_include_directories(DiceVoxSnapshots PRIVATE
        Source $<TARGET_PROPERTY:DiceVox,INCLUDE_DIRECTORIES>)
    target_compile_definitions(DiceVoxSnapshots PRIVATE
        $<TARGET_PROPERTY:DiceVox,COMPILE_DEFINITIONS>)
    target_link_libraries(DiceVoxSnapshots PRIVATE DiceVox)
endif()
