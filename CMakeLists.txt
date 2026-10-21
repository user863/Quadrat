cmake_minimum_required(VERSION 3.22)

# universal on Mac: Apple silicon + Intel (must precede project())
if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures" FORCE)
endif()

project(Quadrat VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# Windows: M_PI lives behind _USE_MATH_DEFINES; keep windows.h off min/max
if (WIN32)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS")

# copy into the system plug-in folders after a build: on by default on Mac;
# off elsewhere (Windows needs admin for Program Files)
if (APPLE)
    option(QUADRAT_INSTALL_AFTER_BUILD "Install plug-ins after building" ON)
else()
    option(QUADRAT_INSTALL_AFTER_BUILD "Install plug-ins after building" OFF)
endif()

# a fresh build folder is a release build unless asked otherwise: without
# this, cmake builds unoptimised and that is what gets installed
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug, Release, RelWithDebInfo or MinSizeRel" FORCE)
endif()

# JUCE 9.0.2, pinned as a git submodule: git submodule update --init
add_subdirectory(third_party/JUCE juce_build)

juce_add_plugin(Quadrat
    PRODUCT_NAME              "Quadrat"
    COMPANY_NAME              "Quadrat"
    BUNDLE_ID                 "io.github.user863.quadrat"
    PLUGIN_MANUFACTURER_CODE  Qdra
    PLUGIN_CODE               Qdrt
    FORMATS                   AU VST3 Standalone
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   ${QUADRAT_INSTALL_AFTER_BUILD}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Quadrat's in block brings a microphone or an input into the patch."
    AU_MAIN_TYPE              kAudioUnitType_MusicDevice)

# ── the vendored Mutable Instruments DSP (MIT; see third_party/mi/NOTICE) ───
file(GLOB_RECURSE MI_SOURCES third_party/mi/*.cc)
add_library(MutableDSP STATIC ${MI_SOURCES})
target_include_directories(MutableDSP PUBLIC third_party/mi)
# TEST swaps their ARM inline assembly for portable C — it is how the library
# is built for its own unit tests.
target_compile_definitions(MutableDSP PUBLIC TEST=1)
target_compile_options(MutableDSP PRIVATE -w -Wno-everything)
set_target_properties(MutableDSP PROPERTIES POSITION_INDEPENDENT_CODE TRUE CXX_STANDARD 14)

juce_add_binary_data(QuadratAssets SOURCES Resources/Inter-Regular.ttf)
set_target_properties(QuadratAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(Quadrat PRIVATE
    Source/Blocks.cpp
    Source/MIBlocks.cpp
    Source/Presets.cpp
    Source/Tour.cpp
    Source/MidiText.cpp
    Source/Panel.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/TourPanel.cpp)

target_compile_definitions(Quadrat PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Quadrat
    PRIVATE QuadratAssets MutableDSP juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

# ── the graph and every block, checked with nothing but a compiler ──────────
add_executable(QuadratPatchTest tests/patch_test.cpp Source/Blocks.cpp Source/MIBlocks.cpp Source/Presets.cpp Source/Tour.cpp Source/MidiText.cpp)
target_link_libraries(QuadratPatchTest PRIVATE MutableDSP)
target_compile_features(QuadratPatchTest PRIVATE cxx_std_17)
enable_testing()
add_test(NAME patch COMMAND QuadratPatchTest)

# ── the panel, rendered offscreen ───────────────────────────────────────────
juce_add_console_app(QuadratSnapshot PRODUCT_NAME "QuadratSnapshot")
target_sources(QuadratSnapshot PRIVATE
    tests/editor_snapshot.cpp
    Source/Blocks.cpp Source/MIBlocks.cpp Source/Presets.cpp Source/Tour.cpp Source/MidiText.cpp Source/Panel.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/TourPanel.cpp)
target_compile_definitions(QuadratSnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(QuadratSnapshot PRIVATE
    MutableDSP QuadratAssets juce::juce_audio_utils juce::juce_gui_extra juce::juce_recommended_config_flags)
