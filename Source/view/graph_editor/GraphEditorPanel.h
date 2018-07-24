#pragma once

#include <ProcessorGraph.h>
#include "PinComponent.h"
#include "GraphEditorTracks.h"
#include "GraphEditorConnectors.h"

class GraphEditorPanel : public Component, public ChangeListener, public ConnectorDragListener, public GraphEditorProcessorContainer {
public:
    GraphEditorPanel(ProcessorGraph &g, Project &project)
            : graph(g), project(project) {
        graph.addChangeListener(this);
        setOpaque(true);

        addAndMakeVisible(*(tracks = std::make_unique<GraphEditorTracks>(project, project.getTracks(), *this, graph)));
        addAndMakeVisible(*(audioOutputProcessor = std::make_unique<GraphEditorProcessor>(project.getAudioOutputProcessorState(), *this, graph)));
        addAndMakeVisible(*(connectors = std::make_unique<GraphEditorConnectors>(project.getConnections(), *this, *this, graph)));
    }

    ~GraphEditorPanel() override {
        graph.removeChangeListener(this);
        draggingConnector = nullptr;
    }

    void paint(Graphics &g) override {
        g.fillAll(findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        connectors->setBounds(r);
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
        connectors->updateConnectors();
    }

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override {
        auto *c = dynamic_cast<GraphEditorConnector *> (e.originalComponent);
        draggingConnector.reset(c);

        if (draggingConnector == nullptr)
            draggingConnector = std::make_unique<GraphEditorConnector>(ValueTree(), *this, *this);

        draggingConnector->setInput(source);
        draggingConnector->setOutput(destination);

        addAndMakeVisible(draggingConnector.get());
        draggingConnector->toFront(false);

        dragConnector(e);
    }

    void dragConnector(const MouseEvent &e) override {
        if (draggingConnector != nullptr) {
            draggingConnector->setTooltip({});

            auto pos = e.getEventRelativeTo(this).position;
            auto connection = draggingConnector->getConnection();

            if (auto *pin = findPinAt(e)) {
                if (connection.source.nodeID == 0 && !pin->isInput) {
                    draggingConnector->setInput(pin->pin);
                } else if (connection.destination.nodeID == 0 && pin->isInput) {
                    draggingConnector->setOutput(pin->pin);
                }

                if (graph.canConnect(connection)) {
                    pos = getLocalPoint(pin->getParentComponent(), pin->getBounds().getCentre()).toFloat();
                    draggingConnector->setTooltip(pin->getTooltip());
                }
            }

            if (connection.source.nodeID == 0)
                draggingConnector->dragStart(pos);
            else
                draggingConnector->dragEnd(pos);
        }
    }

    void endDraggingConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        draggingConnector->setTooltip({});
        auto connection = draggingConnector->getConnection();
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
        } else {
            graph.removeConnection(connection);
        }
    }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        return nodeId == project.getAudioOutputNodeId() ?
               audioOutputProcessor.get() : tracks->getProcessorForNodeId(nodeId);
    }

    ProcessorGraph &graph;
    Project &project;

private:
    std::unique_ptr<GraphEditorConnectors> connectors;
    std::unique_ptr<GraphEditorConnector> draggingConnector;
    std::unique_ptr<GraphEditorTracks> tracks;
    std::unique_ptr<GraphEditorProcessor> audioOutputProcessor;

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
