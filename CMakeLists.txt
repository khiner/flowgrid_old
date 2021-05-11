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
    Source
    modules/libusb/libusb
)

target_sources(FlowGrid PRIVATE
    Source/Main.cpp
    Source/processors/DefaultAudioProcessor.cpp
    Source/usb/libusb/libusb_platform_wrapper.c
    Source/ApplicationPropertiesAndCommandManager.h
    Source/DeviceChangeMonitor.h
    Source/DeviceManagerUtilities.h
    Source/PluginManager.cpp
    Source/ProcessorGraph.cpp
    Source/StatefulAudioProcessorContainer.cpp
    Source/Utilities.h
    Source/ValueTreeObjectList.h
    Source/actions/CreateConnectionAction.cpp
    Source/actions/CreateOrDeleteConnectionsAction.cpp
    Source/actions/CreateProcessorAction.cpp
    Source/actions/CreateTrackAction.cpp
    Source/actions/DefaultConnectProcessorAction.cpp
    Source/actions/DeleteConnectionAction.cpp
    Source/actions/DeleteProcessorAction.cpp
    Source/actions/DeleteSelectedItemsAction.cpp
    Source/actions/DeleteTrackAction.cpp
    Source/actions/DisconnectProcessorAction.cpp
    Source/actions/InsertAction.cpp
    Source/actions/InsertProcessorAction.cpp
    Source/actions/MoveSelectedItemsAction.cpp
    Source/actions/ResetDefaultExternalInputConnectionsAction.cpp
    Source/actions/SelectAction.cpp
    Source/actions/SelectProcessorSlotAction.cpp
    Source/actions/SelectRectangleAction.cpp
    Source/actions/SelectTrackAction.cpp
    Source/actions/SetDefaultConnectionsAllowedAction.cpp
    Source/actions/UpdateAllDefaultConnectionsAction.cpp
    Source/actions/UpdateProcessorDefaultConnectionsAction.cpp
    Source/midi/MidiCommunicator.h
    Source/processors/Arpeggiator.h
    Source/processors/BalanceProcessor.h
    Source/processors/DefaultAudioProcessor.h
    Source/processors/GainProcessor.h
    Source/processors/InternalPluginFormat.h
    Source/processors/MidiInputProcessor.h
    Source/processors/MidiKeyboardProcessor.h
    Source/processors/MidiOutputProcessor.h
    Source/processors/MixerChannelProcessor.h
    Source/processors/ParameterTypesTestProcessor.h
    Source/processors/SineBank.h
    Source/processors/SineSynth.h
    Source/processors/StatefulAudioProcessorWrapper.cpp
    Source/processors/TrackInputProcessor.h
    Source/processors/TrackOutputProcessor.h
    Source/processors/audio_sources/ToneSourceWithParameters.h
    Source/push2/Push2Display.h
    Source/push2/Push2DisplayBridge.h
    Source/push2/Push2MidiCommunicator.cpp
    Source/push2/Push2UsbCommunicator.h
    Source/state/ConnectionsState.cpp
    Source/state/CopiedState.cpp
    Source/state/Identifiers.h
    Source/state/InputState.cpp
    Source/state/OutputState.cpp
    Source/state/Project.cpp
    Source/state/Stateful.h
    Source/state/TracksState.cpp
    Source/state/ViewState.cpp
    Source/usb/UsbCommunicator.cpp
    Source/usb/libusb/config_xcode.h
    Source/view/BasicWindow.h
    Source/view/CustomColourIds.h
    Source/view/PluginWindow.cpp
    Source/view/SelectionEditor.cpp
    Source/view/UiColours.h
    Source/view/context_pane/ContextPane.cpp
    Source/view/graph_editor/ConnectorDragListener.h
    Source/view/graph_editor/GraphEditor.h
    Source/view/graph_editor/GraphEditorChannel.cpp
    Source/view/graph_editor/GraphEditorConnector.cpp
    Source/view/graph_editor/GraphEditorConnectors.h
    Source/view/graph_editor/GraphEditorPanel.cpp
    Source/view/graph_editor/GraphEditorProcessorContainer.h
    Source/view/graph_editor/GraphEditorProcessorLane.cpp
    Source/view/graph_editor/GraphEditorProcessorLanes.h
    Source/view/graph_editor/GraphEditorTrack.cpp
    Source/view/graph_editor/GraphEditorTracks.h
    Source/view/graph_editor/TooltipBar.cpp
    Source/view/graph_editor/processor/BaseGraphEditorProcessor.cpp
    Source/view/graph_editor/processor/LabelGraphEditorProcessor.cpp
    Source/view/graph_editor/processor/ParameterPanelGraphEditorProcessor.cpp
    Source/view/graph_editor/processor/TrackInputGraphEditorProcessor.cpp
    Source/view/graph_editor/processor/TrackOutputGraphEditorProcessor.cpp
    Source/view/parameter_control/ParameterControl.h
    Source/view/parameter_control/level_meter/LevelMeter.h
    Source/view/parameter_control/level_meter/LevelMeterSource.cpp
    Source/view/parameter_control/level_meter/MinimalLevelMeter.cpp
    Source/view/parameter_control/level_meter/SegmentedLevelMeter.cpp
    Source/view/parameter_control/slider/MinimalSliderControl.cpp
    Source/view/parameter_control/slider/SliderControl.h
    Source/view/processor_editor/DraggableValueLabel.h
    Source/view/processor_editor/ParameterDisplayComponent.cpp
    Source/view/processor_editor/ParametersPanel.cpp
    Source/view/processor_editor/ProcessorEditor.cpp
    Source/view/processor_editor/SwitchParameterComponent.cpp
    Source/view/push2/Push2Colours.cpp
    Source/view/push2/Push2Component.cpp
    Source/view/push2/Push2ComponentBase.h
    Source/view/push2/Push2Label.h
    Source/view/push2/Push2Listener.h
    Source/view/push2/Push2MixerView.cpp
    Source/view/push2/Push2NoteModePadLedManager.h
    Source/view/push2/Push2ProcessorSelector.cpp
    Source/view/push2/Push2ProcessorView.cpp
    Source/view/push2/Push2TrackManagingView.cpp
)

target_compile_definitions(FlowGrid PRIVATE
    JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_gui_app` call
    JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_gui_app` call
    JUCE_PLUGINHOST_VST=1
    JUCE_PLUGINHOST_VST3=1
)

juce_add_binary_data(FlowGridBinaryData SOURCES
    Resources/AbletonSansBold-Regular.otf
    Resources/AbletonSansLight-Regular.otf
    Resources/AbletonSansMedium-Regular.otf
    Resources/Audio.png
    Resources/CircleWithLineDiagonal.png
    Resources/CircleWithLineHorizontal.png
    Resources/CircleWithLineVertical.png
    Resources/Midi.png
    Resources/PushStartup.png
)

target_link_libraries(FlowGrid
    PRIVATE
    FlowGridBinaryData
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
