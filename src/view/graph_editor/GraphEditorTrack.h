#pragma once

#include "view/graph_editor/processor/TrackInputGraphEditorProcessor.h"
#include "view/graph_editor/processor/TrackOutputGraphEditorProcessor.h"
#include "GraphEditorProcessorLanes.h"

class GraphEditorTrack : public Component, public ValueTree::Listener, public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTrack(Track *track, View &view, Project &project, ProcessorGraph &processorGraph, PluginManager &pluginManager, ConnectorDragListener &connectorDragListener);

    ~GraphEditorTrack() override;

    Track *getTrack() const { return track; }
    const ValueTree &getState() const { return track->getState(); }

    bool isMaster() const { return track->isMaster(); }

    void paint(Graphics &g) override;
    void resized() override;

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override;
    GraphEditorChannel *findChannelAt(const MouseEvent &e);

private:
    static constexpr int BORDER_WIDTH = 2;

    Track *track;
    View &view;
    Project &project;
    ProcessorGraph &processorGraph;
    std::unique_ptr<TrackInputGraphEditorProcessor> trackInputProcessorView;
    std::unique_ptr<TrackOutputGraphEditorProcessor> trackOutputProcessorView;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorLanes lanes;

    void onColourChanged();
    void trackInputProcessorChanged();
    void trackOutputProcessorChanged();

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override;
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
