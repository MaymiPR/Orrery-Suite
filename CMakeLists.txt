cmake_minimum_required(VERSION 3.22)

project(DevilsParticle VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: static C++ runtime, so the plugins load on any PC without a redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# Measurement tools (hosts for the reference plug-in); not shipped.
option(DP_TOOLS "Build the measurement tools" ON)
if(DP_TOOLS)
    juce_add_console_app(ParamDump PRODUCT_NAME "ParamDump")
    target_sources(ParamDump PRIVATE tools/ParamDump.cpp)
    target_compile_definitions(ParamDump PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ParamDump PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
