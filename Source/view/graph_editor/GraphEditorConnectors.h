#pragma once

#include <state/Project.h>
#include <ValueTreeObjectList.h>
#include "GraphEditorConnector.h"

class GraphEditorConnectors : public Component, public Utilities::ValueTreeObjectList<GraphEditorConnector> {
public:
    explicit GraphEditorConnectors(ConnectionsState &connections, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer &graphEditorProcessorContainer, ProcessorGraph &graph)
            : Utilities::ValueTreeObjectList<GraphEditorConnector>(connections.getState()), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
        rebuildObjects();
        setInterceptsMouseClicks(false, true);
    }

    ~GraphEditorConnectors() override {
        freeObjects();
    }

    void resized() override {}

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::CONNECTION);
    }

    GraphEditorConnector *createNewObject(const ValueTree &v) override {
        auto *connector = new GraphEditorConnector(v, connectorDragListener, graphEditorProcessorContainer);
        addAndMakeVisible(connector);
        return connector;
    }

    void deleteObject(GraphEditorConnector *connector) override {
        delete connector;
    }

    void newObjectAdded(GraphEditorConnector *) override {}

    void objectRemoved(GraphEditorConnector *) override {}

    void objectOrderChanged() override {}

    void updateConnectors() {
        {
            const ScopedLockType sl(arrayLock);
            for (auto *connector : objects) {
                connector->update();
            }
        }
    }

private:
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;
};
