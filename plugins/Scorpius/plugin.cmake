# Scorpius (recreation of Cradle's Orion). Included from the top-level CMakeLists.txt, after shared/shared.cmake.
set(SC_DIR ${CMAKE_CURRENT_LIST_DIR})

set(SC_SOURCES
    ${SC_DIR}/Source/PluginProcessor.cpp
    ${SC_DIR}/Source/PluginEditor.cpp
    ${SC_DIR}/Source/Presets.cpp
    ${SC_DIR}/Source/dsp/Engine.cpp
    ${SC_DIR}/Source/ui/Controls.cpp
    ${SC_DIR}/Source/ui/Stripe.cpp
    ${SUITE_UI_SOURCES})

set(SC_DEFS ${SUITE_DEFS} SUITE_REPOSITORY="${SUITE_REPOSITORY}")
# the snapshot / test executables compile the plug-in sources outside the plug-in wrapper
set(SC_APP_DEFS ${SC_DEFS} JucePlugin_Name="Scorpius" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="${PROJECT_VERSION}")

set(formats VST3 Standalone)
if(APPLE)
    list(APPEND formats AU)
endif()
# The manufacturer and plug-in codes make the plug-in's identity in hosts (saved projects find it by them): keep them.
juce_add_plugin(Scorpius
    COMPANY_NAME "Orrery Suite"
    PLUGIN_MANUFACTURER_CODE Orry
    PLUGIN_CODE Scrp
    FORMATS ${formats}
    PRODUCT_NAME "Scorpius"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE   # the host keeps its transport shortcuts
    VST3_CATEGORIES Fx Dynamics Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    BUNDLE_ID com.orrerysuite.scorpius)
target_sources(Scorpius PRIVATE ${SC_SOURCES})
target_include_directories(Scorpius PRIVATE ${SC_DIR}/Source ${CMAKE_SOURCE_DIR})
target_compile_definitions(Scorpius PUBLIC ${SC_DEFS})
target_link_libraries(Scorpius PRIVATE OrreryUiData juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(AP_CLAP)
    clap_juce_extensions_plugin(TARGET Scorpius
        CLAP_ID "com.orrerysuite.scorpius" CLAP_FEATURES audio-effect distortion compressor stereo)
endif()

# Unit tests (processor, presets, compare):  ctest --test-dir build -C Release
juce_add_console_app(ScorpiusTests PRODUCT_NAME "ScorpiusTests")
target_sources(ScorpiusTests PRIVATE ${SC_DIR}/tests/ScorpiusTests.cpp ${SC_SOURCES})
target_include_directories(ScorpiusTests PRIVATE ${SC_DIR}/Source ${CMAKE_SOURCE_DIR})
target_compile_definitions(ScorpiusTests PRIVATE ${SC_APP_DEFS})
target_link_libraries(ScorpiusTests PRIVATE OrreryUiData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
add_test(NAME ScorpiusTests COMMAND ScorpiusTests)

if(AP_TOOLS)
    # Editor snapshot (PNG) for the visual comparison with the reference (tools/compare_ui.py)
    juce_add_console_app(ScorpiusSnapshot PRODUCT_NAME "ScorpiusSnapshot")
    target_sources(ScorpiusSnapshot PRIVATE ${SC_DIR}/tools/Snapshot.cpp ${SC_SOURCES})
    target_include_directories(ScorpiusSnapshot PRIVATE ${SC_DIR}/Source ${CMAKE_SOURCE_DIR})
    target_compile_definitions(ScorpiusSnapshot PRIVATE ${SC_APP_DEFS})
    target_link_libraries(ScorpiusSnapshot PRIVATE OrreryUiData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    # Offline renders through our processor with every DSP constant open to change (the fits: nulltest/orion.py, 'model')
    juce_add_console_app(ScRender PRODUCT_NAME "ScRender")
    target_sources(ScRender PRIVATE ${SC_DIR}/tools/ScRender.cpp ${SC_SOURCES})
    target_include_directories(ScRender PRIVATE ${SC_DIR}/Source ${CMAKE_SOURCE_DIR})
    target_compile_definitions(ScRender PRIVATE ${SC_APP_DEFS})
    target_link_libraries(ScRender PRIVATE OrreryUiData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
