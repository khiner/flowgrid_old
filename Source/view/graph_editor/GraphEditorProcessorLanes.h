#pragma once

#include "ValueTreeObjectList.h"
#include "GraphEditorProcessorLane.h"

class GraphEditorProcessorLanes : public Component, public ValueTreeObjectList<GraphEditorProcessorLane>,
                                  public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessorLanes(ValueTree state, ViewState &view, TracksState &tracks, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener)
            : ValueTreeObjectList<GraphEditorProcessorLane>(std::move(state)),
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

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR_LANE);
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
    ViewState &view;
    TracksState &tracks;
    ProcessorGraph &processorGraph;
    ConnectorDragListener &connectorDragListener;
};
