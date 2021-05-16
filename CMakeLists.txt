# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
# and https://github.com/juce-framework/JUCE/blob/master/examples/CMake/GuiApp/CMakeLists.txt
# for information on `juce` functions.

cmake_minimum_required(VERSION 3.16)
project(FlowGrid VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}\
    -Wall -Wshorten-64-to-32 -Wstrict-aliasing -Wuninitialized\
    -Wconversion -Wsign-compare -Wint-conversion\
    -Wconditional-uninitialized -Woverloaded-virtual -Wreorder\
    -Wconstant-conversion -Wsign-conversion -Wunused-private-field\
    -Wbool-conversion -Wextra-semi -Wunreachable-code\
    -Wzero-as-null-pointer-constant -Wcast-align\
    -Winconsistent-missing-destructor-override -Wshift-sign-overflow\
    -Wnullable-to-nonnull-conversion -Wno-missing-field-initializers\
    -Wno-ignored-qualifiers -Wswitch-enum -Wpedantic"
)

# TODO
#-Wno-unused-variable
#-Wno-infinite-recursion
#-Werror=implicit-function-declaration
#-Wno-shadow-field-in-constructor-modified
#-Wno-shadow-ivar
#-Wuninitialized
#-Wunused-label
#-Wunused-lambda-capture
#-Wno-shadow-field-in-constructor
#-Wno-shadow
#-Wno-shadow-field

add_subdirectory(modules/JUCE)

juce_set_vst2_sdk_path($ENV{HOME}/SDKs/VST_SDK/VST2_SDK)

juce_add_gui_app(FlowGrid
    BUNDLE_ID com.odangludo.flowgrid
    MICROPHONE_PERMISSION_ENABLED TRUE
    DOCUMENT_BROWSER_ENABLED TRUE
    DOCUMENT_EXTENSIONS smp
    COMPANY_NAME "Karl Hiner"
    COMPANY_WEBSITE www.karlhiner.com
    COMPANY_EMAIL karl.hiner@gmail.com
    PRODUCT_NAME FlowGrid
    PLUGINHOST_AU TRUE
)

# Configure a header file to make CMake settings available to the source code
configure_file(FlowGridConfig.h.in FlowGridConfig.h)

# add the binary tree to the search path for include files so that we will find FlowGridConfig.h
target_include_directories(FlowGrid PRIVATE
    "${PROJECT_BINARY_DIR}"
    src
    modules/libusb/libusb
)

target_sources(FlowGrid PRIVATE
    src/Main.cpp
    src/processors/DefaultAudioProcessor.cpp
    src/usb/libusb/libusb_platform_wrapper.c
    src/ApplicationPropertiesAndCommandManager.h
    src/DeviceChangeMonitor.h
    src/DeviceManagerUtilities.h
    src/PluginManager.cpp
    src/ProcessorGraph.cpp
    src/ValueTreeObjectList.h
    src/actions/CreateConnectionAction.cpp
    src/actions/CreateOrDeleteConnectionsAction.cpp
    src/actions/CreateProcessorAction.cpp
    src/actions/CreateTrackAction.cpp
    src/actions/DefaultConnectProcessorAction.cpp
    src/actions/DeleteConnectionAction.cpp
    src/actions/DeleteProcessorAction.cpp
    src/actions/DeleteSelectedItemsAction.cpp
    src/actions/DeleteTrackAction.cpp
    src/actions/DisconnectProcessorAction.cpp
    src/actions/InsertAction.cpp
    src/actions/InsertProcessorAction.cpp
    src/actions/MoveSelectedItemsAction.cpp
    src/actions/ResetDefaultExternalInputConnectionsAction.cpp
    src/actions/SelectAction.cpp
    src/actions/SelectProcessorSlotAction.cpp
    src/actions/SelectRectangleAction.cpp
    src/actions/SelectTrackAction.cpp
    src/actions/SetDefaultConnectionsAllowedAction.cpp
    src/actions/UpdateAllDefaultConnectionsAction.cpp
    src/actions/UpdateProcessorDefaultConnectionsAction.cpp
    src/midi/MidiCommunicator.h
    src/processors/Arpeggiator.h
    src/processors/BalanceProcessor.h
    src/processors/DefaultAudioProcessor.h
    src/processors/GainProcessor.h
    src/processors/InternalPluginFormat.cpp
    src/processors/MidiInputProcessor.h
    src/processors/MidiKeyboardProcessor.h
    src/processors/MidiOutputProcessor.h
    src/processors/MixerChannelProcessor.h
    src/processors/ParameterTypesTestProcessor.h
    src/processors/SineBank.h
    src/processors/SineSynth.h
    src/processors/StatefulAudioProcessorWrapper.cpp
    src/processors/TrackInputProcessor.h
    src/processors/TrackOutputProcessor.h
    src/processors/audio_sources/ToneSourceWithParameters.h
    src/push2/Push2Display.h
    src/push2/Push2DisplayBridge.h
    src/push2/Push2MidiCommunicator.cpp
    src/push2/Push2UsbCommunicator.h
    src/state/ConnectionState.cpp
    src/state/ConnectionsState.cpp
    src/state/CopiedTracks.cpp
    src/state/InputState.cpp
    src/state/OutputState.cpp
    src/state/InputChannelsState.cpp
    src/state/OutputChannelsState.cpp
    src/state/Project.cpp
    src/state/Stateful.cpp
    src/state/ProcessorState.cpp
    src/state/TrackState.cpp
    src/state/TracksState.cpp
    src/state/ViewState.cpp
    src/usb/UsbCommunicator.cpp
    src/view/BasicWindow.h
    src/view/CustomColourIds.h
    src/view/PluginWindow.cpp
    src/view/SelectionEditor.cpp
    src/view/UiColours.h
    src/view/context_pane/ContextPane.cpp
    src/view/graph_editor/ConnectorDragListener.h
    src/view/graph_editor/GraphEditor.h
    src/view/graph_editor/GraphEditorChannel.cpp
    src/view/graph_editor/GraphEditorConnector.cpp
    src/view/graph_editor/GraphEditorConnectors.h
    src/view/graph_editor/GraphEditorPanel.cpp
    src/view/graph_editor/GraphEditorProcessorContainer.h
    src/view/graph_editor/GraphEditorProcessorLane.cpp
    src/view/graph_editor/GraphEditorProcessorLanes.h
    src/view/graph_editor/GraphEditorTrack.cpp
    src/view/graph_editor/GraphEditorTracks.h
    src/view/graph_editor/TooltipBar.cpp
    src/view/graph_editor/processor/BaseGraphEditorProcessor.cpp
    src/view/graph_editor/processor/LabelGraphEditorProcessor.cpp
    src/view/graph_editor/processor/ParameterPanelGraphEditorProcessor.cpp
    src/view/graph_editor/processor/TrackInputGraphEditorProcessor.cpp
    src/view/graph_editor/processor/TrackOutputGraphEditorProcessor.cpp
    src/view/parameter_control/ParameterControl.h
    src/view/parameter_control/level_meter/LevelMeter.h
    src/view/parameter_control/level_meter/LevelMeterSource.cpp
    src/view/parameter_control/level_meter/MinimalLevelMeter.cpp
    src/view/parameter_control/slider/MinimalSliderControl.cpp
    src/view/parameter_control/slider/SliderControl.h
    src/view/processor_editor/DraggableValueLabel.h
    src/view/processor_editor/ParameterDisplayComponent.cpp
    src/view/processor_editor/ParametersPanel.cpp
    src/view/processor_editor/ProcessorEditor.cpp
    src/view/processor_editor/SwitchParameterComponent.cpp
    src/view/push2/Push2Colours.cpp
    src/view/push2/Push2Component.cpp
    src/view/push2/Push2ComponentBase.h
    src/view/push2/Push2Label.h
    src/view/push2/Push2Listener.h
    src/view/push2/Push2MixerView.cpp
    src/view/push2/Push2NoteModePadLedManager.h
    src/view/push2/Push2ProcessorSelector.cpp
    src/view/push2/Push2ProcessorView.cpp
    src/view/push2/Push2TrackManagingView.cpp
)

target_compile_definitions(FlowGrid PRIVATE
    JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_gui_app` call
    JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_gui_app` call
    JUCE_PLUGINHOST_VST=1
    JUCE_PLUGINHOST_VST3=1
)

juce_add_binary_data(FlowGridBinaryData SOURCES
    assets/AbletonSansBold-Regular.otf
    assets/AbletonSansLight-Regular.otf
    assets/AbletonSansMedium-Regular.otf
    assets/Audio.png
    assets/CircleWithLineDiagonal.png
    assets/CircleWithLineHorizontal.png
    assets/CircleWithLineVertical.png
    assets/Midi.png
    assets/PushStartup.png
)

target_link_libraries(FlowGrid
    PRIVATE
    FlowGridBinaryData
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
