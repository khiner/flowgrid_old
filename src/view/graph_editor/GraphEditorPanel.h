#pragma once

#include "view/PluginWindow.h"
#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "ProcessorGraph.h"
#include "GraphEditorChannel.h"
#include "GraphEditorInput.h"
#include "GraphEditorOutput.h"
#include "GraphEditorTracks.h"
#include "GraphEditorConnectors.h"

class GraphEditorPanel
    : public Component, public ConnectorDragListener, public GraphEditorProcessorContainer,
      private ValueTree::Listener, StatefulList<Track>::Listener, StatefulList<Processor>::Listener {
public:
    GraphEditorPanel(View &view, Tracks &tracks, Connections &connections, Input &input, Output &output, ProcessorGraph &processorGraph, Project &project, PluginManager &pluginManager);

    ~GraphEditorPanel() override;

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override;

    void update() override;

    void mouseDown(const MouseEvent &e) override;
    void mouseDrag(const MouseEvent &e) override;
    void mouseUp(const MouseEvent &e) override;
    void resized() override;

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override;
    void dragConnector(const MouseEvent &e) override;
    void endDraggingConnector(const MouseEvent &e) override;

    bool closeAnyOpenPluginWindows();

private:
    View &view;
    Tracks &tracks;
    Connections &connections;
    Input &input;
    Output &output;

    ProcessorGraph &graph;
    Project &project;

    const AudioProcessorGraph::Connection EMPTY_CONNECTION{{ProcessorGraph::NodeID(0), 0},
                                                           {ProcessorGraph::NodeID(0), 0}};
    std::unique_ptr<GraphEditorConnectors> connectors;
    std::unique_ptr<fg::Connection> draggingConnection;
    std::unique_ptr<GraphEditorConnector> ownedDraggingGraphEditorConnection; // originated from a click on a pin, in which case we create and own the new connector component
    GraphEditorConnector *draggingGraphEditorConnection{}; // link to either `ownedDraggingGraphEditorConnection` or one owned by `connectors`
    GraphEditorInput graphEditorInput;
    GraphEditorOutput graphEditorOutput;
    std::unique_ptr<GraphEditorTracks> graphEditorTracks;
    AudioProcessorGraph::Connection initialDraggingConnection{EMPTY_CONNECTION};
    DrawableRectangle unfocusOverlay;
    OwnedArray<PluginWindow> activePluginWindows;

    juce::Point<int> trackAndSlotAt(const MouseEvent &e);

    int getTrackWidth() { return (getWidth() - View::TRACKS_MARGIN * 2) / View::NUM_VISIBLE_TRACKS; }
    int getProcessorHeight() { return (getHeight() - View::TRACK_LABEL_HEIGHT - View::TRACK_INPUT_HEIGHT) / (View::NUM_VISIBLE_PROCESSOR_SLOTS + 1); }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const;

    ResizableWindow *getOrCreateWindowFor(Processor *processorState, PluginWindowType type);
    void closeWindowFor(const Processor *processor);
    void showPopupMenu(const Track *track, int slot);

    void stopDragging() {
        draggingConnection = nullptr;
        draggingGraphEditorConnection = nullptr;
    }

    void onChildAdded(Track *track) override { connectors->updateConnectors(); }
    void onChildChanged(Track *, const Identifier &) override {}
    void onChildRemoved(Track *track, int oldIndex) override { connectors->updateConnectors(); }
    void onOrderChanged() override { connectors->updateConnectors(); }
    void onChildAdded(Processor *processor) override { connectors->updateConnectors(); }
    void onChildRemoved(Processor *processor, int oldIndex) override { connectors->updateConnectors(); }
    void onChildChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::slot) {
            connectors->updateConnectors();
        } else if (i == ProcessorIDs::pluginWindowType) {
            const auto type = static_cast<PluginWindowType>(processor->getPluginWindowType());
            if (type == PluginWindowType::none) {
                closeWindowFor(processor);
            } else if (auto *w = getOrCreateWindowFor(processor, type)) {
                w->toFront(true);
            }
        }
    }
    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (Connection::isType(child)) connectors->updateConnectors();
    }
    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int oldIndex) override {
        if (Connection::isType(child)) connectors->updateConnectors();
    }
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == ViewIDs::gridSlotOffset) {
            connectors->updateConnectors();
        } else if (i == ViewIDs::gridTrackOffset || i == ViewIDs::masterSlotOffset) {
            resized();
        } else if (i == ViewIDs::focusedPane) {
            unfocusOverlay.setVisible(!view.isGridPaneFocused());
            unfocusOverlay.toFront(false);
        }
    }
};
