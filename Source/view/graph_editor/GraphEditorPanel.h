#pragma once

#include <ProcessorGraph.h>
#include "PinComponent.h"
#include "ConnectorComponent.h"
#include "GraphEditorTracks.h"

class GraphEditorPanel : public Component, public ChangeListener, public ConnectorDragListener {
public:
    GraphEditorPanel(ProcessorGraph &g, Project &project)
            : graph(g), project(project) {
        graph.addChangeListener(this);
        setOpaque(true);

        addAndMakeVisible(*(tracks = std::make_unique<GraphEditorTracks>(project, project.getTracks(), *this, graph)));
        addAndMakeVisible(*(audioOutputProcessor = std::make_unique<GraphEditorProcessor>(project.getAudioOutputProcessorState(), *this, graph)));
    }

    ~GraphEditorPanel() override {
        graph.removeChangeListener(this);
        draggingConnector = nullptr;
        connectors.clear();
    }

    void paint(Graphics &g) override {
        g.fillAll(findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        tracks->setBounds(r.withHeight(int(getHeight() * 8.0 / 9.0)));
        audioOutputProcessor->setBounds(r.removeFromBottom(int(getHeight() * 1.0 / 9.0)));
        updateComponents();
    }

    void changeListenerCallback(ChangeBroadcaster *) override {
        updateComponents();
    }

    void updateComponents() {
        audioOutputProcessor->update();
        updateNodes();

        for (int i = connectors.size(); --i >= 0;)
            if (!graph.isConnectedUi(connectors.getUnchecked(i)->connection))
                connectors.remove(i);

        for (auto *cc : connectors)
            cc->update();

        for (auto &c : graph.getConnectionsUi()) {
            if (getComponentForConnection(c) == nullptr) {
                auto *comp = connectors.add(new ConnectorComponent(*this, graph));
                addAndMakeVisible(comp);

                comp->setInput(c.source, getProcessorForNodeId(c.source.nodeID));
                comp->setOutput(c.destination, getProcessorForNodeId(c.destination.nodeID));
            }
        }
    }

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override {
        auto *c = dynamic_cast<ConnectorComponent *> (e.originalComponent);
        connectors.removeObject(c, false);
        draggingConnector.reset(c);

        if (draggingConnector == nullptr)
            draggingConnector = std::make_unique<ConnectorComponent>(*this, graph);

        draggingConnector->setInput(source, getProcessorForNodeId(source.nodeID));
        draggingConnector->setOutput(destination, getProcessorForNodeId(destination.nodeID));

        addAndMakeVisible(draggingConnector.get());
        draggingConnector->toFront(false);

        dragConnector(e);
    }

    void dragConnector(const MouseEvent &e) override {
        if (draggingConnector != nullptr) {
            draggingConnector->setTooltip({});

            auto pos = e.getEventRelativeTo(this).position;
            
            if (auto *pin = findPinAt(e)) {
                auto connection = draggingConnector->connection;

                if (connection.source.nodeID == 0 && !pin->isInput) {
                    connection.source = pin->pin;
                } else if (connection.destination.nodeID == 0 && pin->isInput) {
                    connection.destination = pin->pin;
                }

                if (graph.canConnect(connection)) {
                    pos = getLocalPoint(pin->getParentComponent(), pin->getBounds().getCentre()).toFloat();
                    draggingConnector->setTooltip(pin->getTooltip());
                }
            }

            if (draggingConnector->connection.source.nodeID == 0)
                draggingConnector->dragStart(pos);
            else
                draggingConnector->dragEnd(pos);
        }
    }

    void endDraggingConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        draggingConnector->setTooltip({});
        auto connection = draggingConnector->connection;
        draggingConnector = nullptr;

        if (auto *pin = findPinAt(e)) {
            if (connection.source.nodeID == 0) {
                if (pin->isInput)
                    return;

                connection.source = pin->pin;
            } else {
                if (!pin->isInput)
                    return;

                connection.destination = pin->pin;
            }

            graph.addConnection(connection);
        }
    }

    ProcessorGraph &graph;
    Project &project;

private:
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<GraphEditorTracks> tracks;
    std::unique_ptr<GraphEditorProcessor> audioOutputProcessor;

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        return nodeId == project.getAudioOutputNodeId() ?
               audioOutputProcessor.get() : tracks->getProcessorForNodeId(nodeId);
    }

    ConnectorComponent *getComponentForConnection(const AudioProcessorGraph::Connection &conn) const {
        for (auto *cc : connectors) {
            if (cc->connection == conn) {
                return cc;
            }
        }
        return nullptr;
    }

    PinComponent *findPinAt(const MouseEvent &e) const {
        if (auto *pin = audioOutputProcessor->findPinAt(e)) {
            return pin;
        }
        return tracks->findPinAt(e);
    }

    void updateNodes() {
        tracks->updateNodes();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
