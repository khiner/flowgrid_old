#pragma once

#include "model/StatefulList.h"
#include "GraphEditorProcessorLane.h"

class GraphEditorProcessorLanes : public Component,
                                  public GraphEditorProcessorContainer,
                                  private ProcessorLanes::Listener {
public:
    explicit GraphEditorProcessorLanes(ProcessorLanes &lanes, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener)
            : lanes(lanes), track(track), view(view), processorWrappers(processorWrappers), connectorDragListener(connectorDragListener) {
        lanes.addProcessorLanesListener(this);
        for (auto *lane : lanes.getChildren()) {
            processorLaneAdded(lane);
        }
    }

    ~GraphEditorProcessorLanes() override {
        lanes.removeProcessorLanesListener(this);
    }

    void processorLaneAdded(ProcessorLane *lane) override {
        addAndMakeVisible(children.insert(lane->getIndex(), new GraphEditorProcessorLane(lane, track, view, processorWrappers, connectorDragListener)));
        resized();
    }
    void processorLaneRemoved(ProcessorLane *processorLane, int oldIndex) override {
        children.remove(oldIndex);
        resized();
    }
    void processorLaneOrderChanged() override {
        children.sort(*this);
        resized();
        connectorDragListener.update();
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

    int compareElements(GraphEditorProcessorLane *first, GraphEditorProcessorLane *second) const {
        return lanes.indexOf(first->getLane()) - lanes.indexOf(second->getLane());
    }

private:
    OwnedArray<GraphEditorProcessorLane> children;

    ProcessorLanes &lanes;
    Track *track;
    View &view;
    StatefulAudioProcessorWrappers &processorWrappers;
    ConnectorDragListener &connectorDragListener;
};
