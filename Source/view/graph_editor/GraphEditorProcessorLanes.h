#pragma once

#include <ValueTreeObjectList.h>
#include <state/Project.h>
#include "JuceHeader.h"
#include "GraphEditorProcessorLane.h"

class GraphEditorProcessorLanes : public Component, public Utilities::ValueTreeObjectList<GraphEditorProcessorLane>,
                                  public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessorLanes(Project &project, ValueTree state, ConnectorDragListener &connectorDragListener)
            : Utilities::ValueTreeObjectList<GraphEditorProcessorLane>(state), project(project),
              connectorDragListener(connectorDragListener) {
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
        auto r = getLocalBounds();

        // Assuming only one lane for now
        if (!objects.isEmpty())
            objects.getFirst()->setBounds(r);
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR_LANE);
    }

    GraphEditorProcessorLane *createNewObject(const ValueTree &v) override {
        auto *lane = new GraphEditorProcessorLane(project, v, connectorDragListener);
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
    Project &project;
    ConnectorDragListener &connectorDragListener;
};
