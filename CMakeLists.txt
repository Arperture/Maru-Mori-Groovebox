cmake_minimum_required(VERSION 3.24)
project(MaruMori VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.14
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Engine core: pure C++, no JUCE dependency. Shared by the plugin and the
# offline render test so DSP can be verified without a host.
add_library(maru_engine STATIC
  Source/engine/GrooveEngine.cpp
  Source/engine/parts/BassPart.cpp
  Source/engine/parts/AcidPart.cpp
  Source/engine/parts/DrumPart.cpp)
target_include_directories(maru_engine PUBLIC Source)

juce_add_plugin(MaruMori
  COMPANY_NAME              "Arperture"
  BUNDLE_ID                 com.arperture.marumori
  PLUGIN_MANUFACTURER_CODE  Arpe
  PLUGIN_CODE               Maru
  FORMATS                   AU VST3 Standalone
  IS_SYNTH                  TRUE
  NEEDS_MIDI_INPUT          TRUE
  NEEDS_MIDI_OUTPUT         FALSE
  COPY_PLUGIN_AFTER_BUILD   TRUE
  PRODUCT_NAME              "Maru Mori")

target_sources(MaruMori PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(MaruMori PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MaruMori
  PRIVATE maru_engine juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(MaruRenderTest tests/RenderTest.cpp)
target_link_libraries(MaruRenderTest PRIVATE maru_engine)
