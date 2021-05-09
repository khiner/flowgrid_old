# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
# and https://github.com/juce-framework/JUCE/blob/master/examples/CMake/GuiApp/CMakeLists.txt
# for information on `juce` functions.

cmake_minimum_required(VERSION 3.16)
project(FlowGrid VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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
    Source/PluginManager.h
    Source/ProcessorGraph.cpp
    Source/StatefulAudioProcessorContainer.h
    Source/Utilities.h
    Source/ValueTreeObjectList.h
    Source/actions/CreateConnectionAction.h
    Source/actions/CreateOrDeleteConnectionsAction.h
    Source/actions/CreateProcessorAction.h
    Source/actions/CreateTrackAction.h
    Source/actions/DefaultConnectProcessorAction.h
    Source/actions/DeleteConnectionAction.h
    Source/actions/DeleteProcessorAction.h
    Source/actions/DeleteSelectedItemsAction.h
    Source/actions/DeleteTrackAction.h
    Source/actions/DisconnectProcessorAction.h
    Source/actions/InsertAction.h
    Source/actions/InsertProcessorAction.h
    Source/actions/MoveSelectedItemsAction.h
    Source/actions/ResetDefaultExternalInputConnectionsAction.h
    Source/actions/SelectAction.h
    Source/actions/SelectProcessorSlotAction.h
    Source/actions/SelectRectangleAction.h
    Source/actions/SelectTrackAction.h
    Source/actions/SetDefaultConnectionsAllowedAction.h
    Source/actions/UpdateAllDefaultConnectionsAction.h
    Source/actions/UpdateProcessorDefaultConnectionsAction.h
    Source/config.h
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
    Source/processors/StatefulAudioProcessorWrapper.h
    Source/processors/TrackInputProcessor.h
    Source/processors/TrackOutputProcessor.h
    Source/processors/audio_sources/ToneSourceWithParameters.h
    Source/push2/Push2Display.h
    Source/push2/Push2DisplayBridge.h
    Source/push2/Push2MidiCommunicator.h
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
    Source/usb/UsbCommunicator.h
    Source/usb/libusb/config_msvc.h
    Source/usb/libusb/config_xcode.h
    Source/view/BasicWindow.h
    Source/view/CustomColourIds.h
    Source/view/PluginWindow.cpp
    Source/view/SelectionEditor.cpp
    Source/view/UiColours.h
    Source/view/context_pane/ContextPane.h
    Source/view/graph_editor/ConnectorDragListener.h
    Source/view/graph_editor/GraphEditor.h
    Source/view/graph_editor/GraphEditorChannel.h
    Source/view/graph_editor/GraphEditorConnector.h
    Source/view/graph_editor/GraphEditorConnectors.h
    Source/view/graph_editor/GraphEditorPanel.h
    Source/view/graph_editor/GraphEditorProcessorContainer.h
    Source/view/graph_editor/GraphEditorProcessorLane.h
    Source/view/graph_editor/GraphEditorProcessorLanes.h
    Source/view/graph_editor/GraphEditorTrack.h
    Source/view/graph_editor/GraphEditorTracks.h
    Source/view/graph_editor/TooltipBar.h
    Source/view/graph_editor/processor/BaseGraphEditorProcessor.h
    Source/view/graph_editor/processor/LabelGraphEditorProcessor.h
    Source/view/graph_editor/processor/ParameterPanelGraphEditorProcessor.h
    Source/view/graph_editor/processor/TrackInputGraphEditorProcessor.h
    Source/view/graph_editor/processor/TrackOutputGraphEditorProcessor.h
    Source/view/parameter_control/ParameterControl.h
    Source/view/parameter_control/level_meter/LevelMeter.h
    Source/view/parameter_control/level_meter/LevelMeterSource.h
    Source/view/parameter_control/level_meter/MinimalLevelMeter.h
    Source/view/parameter_control/level_meter/SegmentedLevelMeter.h
    Source/view/parameter_control/slider/MinimalSliderControl.h
    Source/view/parameter_control/slider/SliderControl.h
    Source/view/processor_editor/DraggableValueLabel.h
    Source/view/processor_editor/ParameterDisplayComponent.h
    Source/view/processor_editor/ParametersPanel.h
    Source/view/processor_editor/ProcessorEditor.h
    Source/view/processor_editor/SwitchParameterComponent.h
    Source/view/push2/Push2Colours.h
    Source/view/push2/Push2Component.h
    Source/view/push2/Push2ComponentBase.h
    Source/view/push2/Push2Label.h
    Source/view/push2/Push2Listener.h
    Source/view/push2/Push2MixerView.h
    Source/view/push2/Push2NoteModePadLedManager.h
    Source/view/push2/Push2ProcessorSelector.h
    Source/view/push2/Push2ProcessorView.h
    Source/view/push2/Push2TrackManagingView.h
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
    juce::juce_recommended_warning_flags
)
