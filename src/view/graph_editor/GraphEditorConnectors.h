#pragma once

#include "model/StatefulList.h"
#include "GraphEditorConnector.h"

class GraphEditorConnectors : public Component, private StatefulList<Connection>::Listener {
public:
    explicit GraphEditorConnectors(Connections &connections, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer &graphEditorProcessorContainer)
            : connections(connections), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer) {
        setInterceptsMouseClicks(false, true);
        connections.addChildListener(this);
    }

    ~GraphEditorConnectors() override {
        connections.removeChildListener(this);
    }

    void resized() override {}

    void updateConnectors() {
        for (auto *connector : children) {
            connector->update();
        }
    }

    void onChildAdded(Connection *connection) override {
        addAndMakeVisible(children.insert(connection->getIndex(), new GraphEditorConnector(connection, connectorDragListener, graphEditorProcessorContainer)));
        resized();
    }
    void onChildRemoved(Connection *, int oldIndex) override {
        children.remove(oldIndex);
        resized();
    }
    void onOrderChanged() override {
        children.sort(*this);
        resized();
        connectorDragListener.update();
    }

    int compareElements(GraphEditorConnector *first, GraphEditorConnector *second) const {
        return connections.indexOf(first->getConnection()) - connections.indexOf(second->getConnection());
    }

private:
    OwnedArray<GraphEditorConnector> children;

    Connections &connections;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;
};
