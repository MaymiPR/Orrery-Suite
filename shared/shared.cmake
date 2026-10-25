# Code and resources every Orrery Suite plug-in uses: the editor's theme and font, the top-bar controls, menus,
# dialogs and the measured metal knob. Included from the top-level CMakeLists.txt before the plug-ins.
# A plug-in adds ${SUITE_UI_SOURCES} to its sources, ${CMAKE_SOURCE_DIR} to its include path (#include
# "shared/ui/...") and links OrreryUiData.
set(SUITE_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR})

juce_add_binary_data(OrreryUiData NAMESPACE OrreryUiData HEADER_NAME OrreryUiData.h
    SOURCES ${SUITE_SHARED_DIR}/resources/fonts/InstrumentSans-Regular.ttf)
set_target_properties(OrreryUiData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SUITE_UI_SOURCES
    ${SUITE_SHARED_DIR}/ui/MetalKnob.cpp
    ${SUITE_SHARED_DIR}/ui/Widgets.cpp)
