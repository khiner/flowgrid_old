#pragma once

#include <ValueTreeObjectList.h>
#include <state/Project.h>
#include "JuceHeader.h"
#include "GraphEditorProcessorLane.h"

class GraphEditorProcessorLanes : public Component, public Utilities::ValueTreeObjectList<GraphEditorProcessorLane> {
public:
    explicit GraphEditorProcessorLanes(Project &project, ValueTree &state, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer &graphEditorProcessorContainer)
            : Utilities::ValueTreeObjectList<GraphEditorProcessorLane>(state), project(project),
              connectorDragListener(connectorDragListener) {
        rebuildObjects();
        setInterceptsMouseClicks(false, true);
    }

    ~GraphEditorProcessorLanes() override {
        freeObjects();
    }

    void resized() override {}

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::CONNECTION);
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
