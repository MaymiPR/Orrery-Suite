# The Antiparticle (recreation of Cradle's The God Particle). Included from the top-level CMakeLists.txt.
set(AP_DIR ${CMAKE_CURRENT_LIST_DIR})

juce_add_binary_data(AntiparticleData SOURCES ${AP_DIR}/resources/fonts/InstrumentSans-Regular.ttf
    ${AP_DIR}/resources/glass/glass_off.png ${AP_DIR}/resources/glass/glass_lit.png)
set_target_properties(AntiparticleData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(AP_SOURCES
    ${AP_DIR}/Source/dsp/Engine.cpp
    ${AP_DIR}/Source/PluginProcessor.cpp
    ${AP_DIR}/Source/PluginEditor.cpp
    ${AP_DIR}/Source/Presets.cpp
    ${AP_DIR}/Source/ui/MetalKnob.cpp
    ${AP_DIR}/Source/ui/Visualizer.cpp
    ${AP_DIR}/Source/ui/Widgets.cpp)

set(AP_DEFS ${NS_DEFS} NS_REPOSITORY="${NS_REPOSITORY}")
# the snapshot / test executables compile the plug-in sources outside the plug-in wrapper
set(AP_APP_DEFS ${AP_DEFS} JucePlugin_Name="The Antiparticle" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="${PROJECT_VERSION}")

set(formats VST3 Standalone)
if(APPLE)
    list(APPEND formats AU)
endif()
# The manufacturer and plug-in codes make the plug-in's identity in hosts (saved projects find it by them): keep them.
juce_add_plugin(TheAntiparticle
    COMPANY_NAME "Newton Suite"
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
    BUNDLE_ID com.newtonsuite.theantiparticle)
# MSBuild cannot handle the apostrophe of the product name in the shared-code library's file name
set_target_properties(TheAntiparticle PROPERTIES OUTPUT_NAME "TheAntiparticle_SharedCode")
target_sources(TheAntiparticle PRIVATE ${AP_SOURCES})
target_include_directories(TheAntiparticle PRIVATE ${AP_DIR}/Source)
target_compile_definitions(TheAntiparticle PUBLIC ${AP_DEFS})
target_link_libraries(TheAntiparticle PRIVATE AntiparticleData juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(AP_CLAP)
    clap_juce_extensions_plugin(TARGET TheAntiparticle
        CLAP_ID "com.newtonsuite.theantiparticle" CLAP_FEATURES audio-effect mastering stereo)
endif()

# Unit tests (DSP + processor):  cmake --build build --target AntiparticleTests, then ctest --test-dir build -C Release
juce_add_console_app(AntiparticleTests PRODUCT_NAME "AntiparticleTests")
target_sources(AntiparticleTests PRIVATE ${AP_DIR}/tests/AntiparticleTests.cpp ${AP_SOURCES})
target_include_directories(AntiparticleTests PRIVATE ${AP_DIR}/Source)
target_compile_definitions(AntiparticleTests PRIVATE ${AP_APP_DEFS})
target_link_libraries(AntiparticleTests PRIVATE AntiparticleData juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags)
add_test(NAME AntiparticleTests COMMAND AntiparticleTests)

if(AP_TOOLS)
    # Editor snapshot (PNG) for the visual comparison with the reference
    juce_add_console_app(AntiparticleSnapshot PRODUCT_NAME "AntiparticleSnapshot")
    target_sources(AntiparticleSnapshot PRIVATE ${AP_DIR}/tools/Snapshot.cpp ${AP_SOURCES})
    target_include_directories(AntiparticleSnapshot PRIVATE ${AP_DIR}/Source)
    target_compile_definitions(AntiparticleSnapshot PRIVATE ${AP_APP_DEFS})
    target_link_libraries(AntiparticleSnapshot PRIVATE AntiparticleData juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)

    # Offline renders through our own processor with every DSP constant open to change (fitting, null tests), and a
    # second copy for trying DSP changes while a fit is running with the first (nulltest/tgp.py: AP_RENDER=dev)
    foreach(t ApRender ApRenderDev)
        juce_add_console_app(${t} PRODUCT_NAME "${t}")
        target_sources(${t} PRIVATE ${AP_DIR}/tools/ApRender.cpp ${AP_SOURCES})
        target_include_directories(${t} PRIVATE ${AP_DIR}/Source)
        target_compile_definitions(${t} PRIVATE ${AP_APP_DEFS})
        target_link_libraries(${t} PRIVATE AntiparticleData juce::juce_audio_utils juce::juce_dsp
            juce::juce_recommended_config_flags)
    endforeach()

    # the band gain computer as a plain C library for the fitting scripts (nulltest/charsim.py)
    add_library(charsim SHARED ${AP_DIR}/tools/charsim.cpp)
    target_include_directories(charsim PRIVATE ${AP_DIR}/Source)
    set_target_properties(charsim PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/charsim)
endif()
