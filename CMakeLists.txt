cmake_minimum_required(VERSION 3.22)

project(TheAntiparticle VERSION 0.1.0)

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
option(AP_CLAP "Build the CLAP plugin" ON)
if(AP_CLAP)
    FetchContent_Declare(clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
        GIT_SUBMODULES_RECURSE ON)
    FetchContent_MakeAvailable(clap_juce_extensions)
endif()

juce_add_binary_data(AntiparticleData SOURCES resources/fonts/InstrumentSans-Regular.ttf
    resources/glass/glass_off.png resources/glass/glass_lit.png)
set_target_properties(AntiparticleData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(AP_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/ui/MetalKnob.cpp
    Source/ui/Visualizer.cpp
    Source/ui/Widgets.cpp)

set(AP_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
# the snapshot / test executables compile the plug-in sources outside the plug-in wrapper
set(AP_APP_DEFS ${AP_DEFS} JucePlugin_Name="The Antiparticle" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="${PROJECT_VERSION}")

set(formats VST3 Standalone)
if(APPLE)
    list(APPEND formats AU)
endif()
juce_add_plugin(TheAntiparticle
    COMPANY_NAME "The Antiparticle"
    PLUGIN_MANUFACTURER_CODE Antp
    PLUGIN_CODE Atpl
    FORMATS ${formats}
    PRODUCT_NAME "The Antiparticle"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE   # the host keeps its transport shortcuts
    VST3_CATEGORIES Fx Dynamics Mastering
    AU_MAIN_TYPE kAudioUnitType_Effect
    BUNDLE_ID com.theantiparticle.theantiparticle)
# MSBuild cannot handle the apostrophe of the product name in the shared-code library's file name
set_target_properties(TheAntiparticle PROPERTIES OUTPUT_NAME "TheAntiparticle_SharedCode")
target_sources(TheAntiparticle PRIVATE ${AP_SOURCES})
target_include_directories(TheAntiparticle PRIVATE Source)
target_compile_definitions(TheAntiparticle PUBLIC ${AP_DEFS})
target_link_libraries(TheAntiparticle PRIVATE AntiparticleData juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(AP_CLAP)
    clap_juce_extensions_plugin(TARGET TheAntiparticle
        CLAP_ID "com.theantiparticle.theantiparticle" CLAP_FEATURES audio-effect mastering stereo)
endif()

# Editor snapshot (PNG) for the visual comparison with the reference
juce_add_console_app(AntiparticleSnapshot PRODUCT_NAME "AntiparticleSnapshot")
target_sources(AntiparticleSnapshot PRIVATE tools/Snapshot.cpp ${AP_SOURCES})
target_include_directories(AntiparticleSnapshot PRIVATE Source)
target_compile_definitions(AntiparticleSnapshot PRIVATE ${AP_APP_DEFS})
target_link_libraries(AntiparticleSnapshot PRIVATE AntiparticleData juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags)

# Measurement tools (hosts for the reference plug-in); not shipped.
option(AP_TOOLS "Build the measurement tools" ON)
if(AP_TOOLS)
    juce_add_console_app(ParamDump PRODUCT_NAME "ParamDump")
    target_sources(ParamDump PRIVATE tools/ParamDump.cpp)
    target_compile_definitions(ParamDump PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ParamDump PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    if(WIN32)
        # films / screenshots the reference editor while audio plays (tools/RefCapture.cpp)
        juce_add_gui_app(RefCapture PRODUCT_NAME "RefCapture")
        target_sources(RefCapture PRIVATE tools/RefCapture.cpp)
        target_compile_definitions(RefCapture PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        target_link_libraries(RefCapture PRIVATE juce::juce_audio_processors juce::juce_gui_basics
            juce::juce_recommended_config_flags)
    endif()
endif()
