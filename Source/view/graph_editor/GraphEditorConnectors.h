#pragma once

#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include <ProcessorGraph.h>
#include "JuceHeader.h"
#include "GraphEditorConnector.h"

class GraphEditorConnectors : public Component, public Utilities::ValueTreeObjectList<GraphEditorConnector> {
public:
    explicit GraphEditorConnectors(const ValueTree &state, ConnectorDragListener &connectorDragListener, GraphEditorProcessorContainer& graphEditorProcessorContainer, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorConnector>(state), connectorDragListener(connectorDragListener), graphEditorProcessorContainer(graphEditorProcessorContainer), graph(graph) {
        rebuildObjects();
    }

    ~GraphEditorConnectors() override {
        freeObjects();
    }

    bool hitTest(int x, int y) override {
        for (auto *connector : objects) {
            if (connector->hitTest(x, y))
                return true;
        }
        return false;
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
        for (auto *connector : objects) {
            connector->update();
        }
    }

    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorContainer &graphEditorProcessorContainer;
    ProcessorGraph &graph;
};
