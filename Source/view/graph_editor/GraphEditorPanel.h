#pragma once

#include "view/PluginWindow.h"
#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "ProcessorGraph.h"
#include "GraphEditorChannel.h"
#include "GraphEditorTracks.h"
#include "GraphEditorConnectors.h"

class GraphEditorPanel
        : public Component, public ConnectorDragListener, public GraphEditorProcessorContainer,
          private ValueTree::Listener {
public:
    GraphEditorPanel(ViewState &view, TracksState &tracks, ConnectionsState &connections, ProcessorGraph &processorGraph, Project &project, PluginManager &pluginManager);

    ~GraphEditorPanel() override;

    void mouseDown(const MouseEvent &e) override;

    void mouseDrag(const MouseEvent &e) override;

    void mouseUp(const MouseEvent &e) override;

    void resized() override;

    void update() override;

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override;

    void dragConnector(const MouseEvent &e) override;

    void endDraggingConnector(const MouseEvent &e) override;

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override;

    bool closeAnyOpenPluginWindows();

private:
    ViewState &view;
    TracksState &tracks;
    ConnectionsState &connections;
    InputState &input;
    OutputState &output;

    ProcessorGraph &graph;
    Project &project;

    const AudioProcessorGraph::Connection EMPTY_CONNECTION{{ProcessorGraph::NodeID(0), 0},
                                                           {ProcessorGraph::NodeID(0), 0}};
    std::unique_ptr<GraphEditorConnectors> connectors;
    GraphEditorConnector *draggingConnector{};
    std::unique_ptr<GraphEditorTracks> graphEditorTracks;
    std::unique_ptr<LabelGraphEditorProcessor> audioInputProcessor;
    std::unique_ptr<LabelGraphEditorProcessor> audioOutputProcessor;
    OwnedArray<LabelGraphEditorProcessor> midiInputProcessors;
    OwnedArray<LabelGraphEditorProcessor> midiOutputProcessors;

    AudioProcessorGraph::Connection initialDraggingConnection{EMPTY_CONNECTION};

    LabelGraphEditorProcessor::ElementComparator processorComparator;

    DrawableRectangle unfocusOverlay;

    OwnedArray<PluginWindow> activePluginWindows;

    int getTrackWidth() { return (getWidth() - ViewState::TRACKS_MARGIN * 2) / ViewState::NUM_VISIBLE_TRACKS; }

    int getProcessorHeight() { return (getHeight() - ViewState::TRACK_LABEL_HEIGHT - ViewState::TRACK_INPUT_HEIGHT) / (ViewState::NUM_VISIBLE_PROCESSOR_SLOTS + 1); }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const;

    LabelGraphEditorProcessor *findMidiInputProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const;

    LabelGraphEditorProcessor *findMidiOutputProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const;

    ResizableWindow *getOrCreateWindowFor(ValueTree &processorState, PluginWindow::Type type);

    void closeWindowFor(ValueTree &processor);

    juce::Point<int> trackAndSlotAt(const MouseEvent &e);

    void showPopupMenu(const ValueTree &track, int slot);

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override;

    void valueTreeChildOrderChanged(ValueTree &parent, int oldIndex, int newIndex) override;
};
