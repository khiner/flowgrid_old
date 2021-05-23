#pragma once

#include "model/StatefulList.h"
#include "GraphEditorConnector.h"

class GraphEditorConnectors : public Component, public StatefulList<GraphEditorConnector> {
public:
    explicit GraphEditorConnectors(Connections &connections, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer &graphEditorProcessorContainer, ProcessorGraph &graph)
            : StatefulList<GraphEditorConnector>(connections.getState()), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
        rebuildObjects();
        setInterceptsMouseClicks(false, true);
    }

    ~GraphEditorConnectors() override {
        freeObjects();
    }

    void resized() override {}

    bool isChildType(const ValueTree &v) const override { return Connection::isType(v); }

    GraphEditorConnector *createNewObject(const ValueTree &v) override {
        auto *connector = new GraphEditorConnector(v, connectorDragListener, graphEditorProcessorContainer);
        addAndMakeVisible(connector);
        return connector;
    }

    void deleteObject(GraphEditorConnector *connector) override { delete connector; }
    void newObjectAdded(GraphEditorConnector *) override {}
    void objectRemoved(GraphEditorConnector *, int) override {}
    void objectOrderChanged() override {}
    void updateConnectors() {
        {
            const ScopedLockType sl(arrayLock);
            for (auto *connector : children) {
                connector->update();
            }
        }
    }

private:
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;
};
