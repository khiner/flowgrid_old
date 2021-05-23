#pragma once

#include "model/StatefulList.h"
#include "GraphEditorProcessorLane.h"

class GraphEditorProcessorLanes : public Component, public StatefulList<GraphEditorProcessorLane>,
                                  public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessorLanes(ValueTree state, View &view, Tracks &tracks, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener)
            : StatefulList<GraphEditorProcessorLane>(std::move(state)),
              view(view), tracks(tracks), processorGraph(processorGraph), connectorDragListener(connectorDragListener) {
        rebuildObjects();
    }

    ~GraphEditorProcessorLanes() override {
        freeObjects();
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *lane : objects)
            if (auto *processor = lane->getProcessorForNodeId(nodeId))
                return processor;
        return nullptr;
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) {
        for (auto *lane : objects)
            if (auto *channel = lane->findChannelAt(e))
                return channel;
        return nullptr;
    }

    void resized() override {
        // Assuming only one lane for now
        if (!objects.isEmpty())
            objects.getFirst()->setBounds(getLocalBounds());
    }

    bool isChildType(const ValueTree &v) const override {
        return v.hasType(ProcessorLaneIDs::PROCESSOR_LANE);
    }

    GraphEditorProcessorLane *createNewObject(const ValueTree &v) override {
        auto *lane = new GraphEditorProcessorLane(v, view, tracks, processorGraph, connectorDragListener);
        addAndMakeVisible(lane);
        return lane;
    }

    void deleteObject(GraphEditorProcessorLane *lane) override {
        delete lane;
    }

    void newObjectAdded(GraphEditorProcessorLane *) override {}

    void objectRemoved(GraphEditorProcessorLane *) override {}

    void objectOrderChanged() override {}

private:
    View &view;
    Tracks &tracks;
    ProcessorGraph &processorGraph;
    ConnectorDragListener &connectorDragListener;
};
