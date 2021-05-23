#pragma once

#include "model/StatefulList.h"
#include "GraphEditorProcessorLane.h"

class GraphEditorProcessorLanes : public Component, public StatefulList<GraphEditorProcessorLane>,
                                  public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessorLanes(ValueTree state, Track *track, View &view, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener)
            : StatefulList<GraphEditorProcessorLane>(std::move(state)), track(track), view(view), processorGraph(processorGraph), connectorDragListener(connectorDragListener) {
        rebuildObjects();
    }

    ~GraphEditorProcessorLanes() override {
        freeObjects();
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *lane : children)
            if (auto *processor = lane->getProcessorForNodeId(nodeId))
                return processor;
        return nullptr;
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) {
        for (auto *lane : children)
            if (auto *channel = lane->findChannelAt(e))
                return channel;
        return nullptr;
    }

    void resized() override {
        // Assuming only one lane for now
        if (!children.isEmpty())
            children.getFirst()->setBounds(getLocalBounds());
    }

    bool isChildType(const ValueTree &v) const override {
        return v.hasType(ProcessorLaneIDs::PROCESSOR_LANE);
    }

    GraphEditorProcessorLane *createNewObject(const ValueTree &v) override {
        auto *lane = new GraphEditorProcessorLane(v, track, view, processorGraph, connectorDragListener);
        addAndMakeVisible(lane);
        return lane;
    }

    void deleteObject(GraphEditorProcessorLane *lane) override {
        delete lane;
    }

    void newObjectAdded(GraphEditorProcessorLane *) override {}

    void objectRemoved(GraphEditorProcessorLane *, int oldIndex) override {}

    void objectOrderChanged() override {}

private:
    Track *track;
    View &view;
    ProcessorGraph &processorGraph;
    ConnectorDragListener &connectorDragListener;
};
