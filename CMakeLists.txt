cmake_minimum_required(VERSION 3.22)
project(GODATOM VERSION 1.0.0 LANGUAGES C CXX)
add_compile_options(-Wno-date-time -Wno-error=date-time)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

add_executable(godatom_engine_tests tests/test_engine.cpp)
target_include_directories(godatom_engine_tests PRIVATE src/engine tests)
add_test(NAME engine COMMAND godatom_engine_tests)

option(GODATOM_ENGINE_ONLY "Build dependency-free engine tests only" OFF)
if(GODATOM_ENGINE_ONLY)
  return()
endif()

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(GODATOM_FORMATS VST3)

juce_add_plugin(GodAtom
  VERSION 1.0.0
  COMPANY_NAME koobacorp
  COMPANY_WEBSITE "https://github.com/koobacorp"
  BUNDLE_ID com.koobacorp.godatom
  PLUGIN_MANUFACTURER_CODE Koob
  PLUGIN_CODE Gdat
  FORMATS ${GODATOM_FORMATS}
  PRODUCT_NAME "GOD ATOM"
  DESCRIPTION "GOD ATOM mix-bus processor"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Dynamics
  AU_MAIN_TYPE kAudioUnitType_Effect
  AU_SANDBOX_SAFE TRUE)
target_sources(GodAtom PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
target_include_directories(GodAtom PUBLIC src/engine src/plugin)
target_compile_definitions(GodAtom PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_USE_CAMERA=0 JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(GodAtom PRIVATE juce::juce_audio_utils juce::juce_gui_basics
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
set_target_properties(GodAtom PROPERTIES VISIBILITY_INLINES_HIDDEN ON CXX_VISIBILITY_PRESET hidden)
if(APPLE)
  target_link_options(GodAtom PUBLIC -Wl,-dead_strip)
endif()

option(GODATOM_BUILD_TOOLS "Build JUCE plugin tests and UI screenshots" ON)
if(GODATOM_BUILD_TOOLS)
  juce_add_console_app(godatom_plugin_tests PRODUCT_NAME godatom_plugin_tests)
  target_sources(godatom_plugin_tests PRIVATE tests/plugin_tests.cpp)
  target_include_directories(godatom_plugin_tests PRIVATE src/engine src/plugin $<TARGET_PROPERTY:GodAtom,INCLUDE_DIRECTORIES>)
  target_compile_definitions(godatom_plugin_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 $<TARGET_PROPERTY:GodAtom,COMPILE_DEFINITIONS>)
  target_link_libraries(godatom_plugin_tests PRIVATE GodAtom juce::juce_recommended_config_flags)
  target_compile_options(GodAtom PRIVATE -Wno-date-time -Wno-unused-parameter -Wno-float-equal)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shots)
  add_test(NAME plugin_tests COMMAND godatom_plugin_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/shots)
endif()
