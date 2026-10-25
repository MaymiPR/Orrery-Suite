cmake_minimum_required(VERSION 3.22)

# Orrery Suite: one CMake project for every plug-in (plugins/<Name>/plugin.cmake) and the measurement tools.
# The suite's version, shared by every plug-in: a new plug-in raises the middle number (1.1.0, 1.2.0 ...), fixes and
# small updates the last (1.1.1, 1.1.2 ...). CHANGELOG.md has a section per version.
project(OrrerySuite VERSION 1.0.1)

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

# CLAP through clap-juce-extensions (MIT; brings the CLAP SDK).
option(AP_CLAP "Build the CLAP plugins" ON)
if(AP_CLAP)
    FetchContent_Declare(clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
        GIT_SUBMODULES_RECURSE ON)
    FetchContent_MakeAvailable(clap_juce_extensions)
endif()

# Measurement tools (hosts for the reference plug-ins, fitting helpers); not shipped.
option(AP_TOOLS "Build the measurement tools" ON)

set(SUITE_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
set(SUITE_REPOSITORY "https://github.com/MaymiPR/OrrerySuite")
enable_testing()

# The plug-ins. Each is included (not added as a subdirectory), so its build output stays at
# build/<Target>_artefacts. packaging/plugins.txt lists them for the installers and the release workflow.
include(shared/shared.cmake)
include(plugins/TheAntiparticle/plugin.cmake)

if(AP_TOOLS)
    juce_add_console_app(ParamDump PRODUCT_NAME "ParamDump")
    target_sources(ParamDump PRIVATE tools/ParamDump.cpp)
    target_compile_definitions(ParamDump PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ParamDump PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    # offline renders through any VST3 (the measurements and null tests; tools/RefRender.cpp)
    juce_add_console_app(RefRender PRODUCT_NAME "RefRender")
    target_sources(RefRender PRIVATE tools/RefRender.cpp)
    target_compile_definitions(RefRender PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RefRender PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
    if(WIN32)
        # films / screenshots a plug-in's editor while audio plays (tools/RefCapture.cpp)
        juce_add_gui_app(RefCapture PRODUCT_NAME "RefCapture")
        target_sources(RefCapture PRIVATE tools/RefCapture.cpp)
        target_compile_definitions(RefCapture PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        target_link_libraries(RefCapture PRIVATE juce::juce_audio_processors juce::juce_gui_basics
            juce::juce_recommended_config_flags)
    endif()
endif()
