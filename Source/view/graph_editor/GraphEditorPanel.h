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

        addAndMakeVisible(*(audioInputProcessor = std::make_unique<GraphEditorProcessor>(project.getAudioInputProcessorState(), *this, graph)));
        addAndMakeVisible(*(audioOutputProcessor = std::make_unique<GraphEditorProcessor>(project.getAudioOutputProcessorState(), *this, graph)));
        addAndMakeVisible(*(tracks = std::make_unique<GraphEditorTracks>(project, project.getTracks(), *this, graph)));
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
        audioInputProcessor->setBounds(r.removeFromTop(int(getHeight() * 1.0 / Project::NUM_VISIBLE_PROCESSOR_SLOTS)));
        tracks->setBounds(r.withHeight(getHeight() * Project::NUM_AVAILABLE_PROCESSOR_SLOTS / (Project::NUM_VISIBLE_PROCESSOR_SLOTS - 1)));
        audioOutputProcessor->setBounds(r.removeFromBottom(int(getHeight() * 1.0 / Project::NUM_VISIBLE_PROCESSOR_SLOTS)));
        updateComponents();
    }

    void changeListenerCallback(ChangeBroadcaster *) override {
        updateComponents();
    }

    void update() override {
        updateComponents();
    }

    void updateComponents() {
        audioInputProcessor->update();
        audioOutputProcessor->update();
        tracks->updateProcessors();
        connectors->updateConnectors();
    }

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override {
        auto *c = dynamic_cast<GraphEditorConnector *> (e.originalComponent);
        draggingConnector = c;

        if (draggingConnector == nullptr)
            draggingConnector = new GraphEditorConnector(ValueTree(), *this, *this);
        else
            initialDraggingConnection = draggingConnector->getConnection();
        draggingConnector->setInput(source);
        draggingConnector->setOutput(destination);

        addAndMakeVisible(draggingConnector);
        draggingConnector->toFront(false);

        dragConnector(e);
    }

    void dragConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        draggingConnector->setTooltip({});

        auto pos = e.getEventRelativeTo(this).position;
        auto connection = draggingConnector->getConnection();

        if (auto *pin = findPinAt(e)) {
            if (connection.source.nodeID == 0 && !pin->isInput)
                connection.source = pin->pin;
            else if (connection.destination.nodeID == 0 && pin->isInput)
                connection.destination = pin->pin;

            if (graph.canConnect(connection) || graph.isConnected(connection)) {
                pos = getLocalPoint(pin->getParentComponent(), pin->getBounds().getCentre()).toFloat();
                draggingConnector->setTooltip(pin->getTooltip());
            }
        }

        if (draggingConnector->getConnection().source.nodeID == 0)
            draggingConnector->dragStart(pos);
        else
            draggingConnector->dragEnd(pos);
    }

    void endDraggingConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        auto newConnection = EMPTY_CONNECTION;
        if (auto *pin = findPinAt(e)) {
            newConnection = draggingConnector->getConnection();
            if (newConnection.source.nodeID == 0) {
                if (pin->isInput)
                    newConnection = EMPTY_CONNECTION;
                else
                    newConnection.source = pin->pin;
            } else {
                if (!pin->isInput)
                    newConnection = EMPTY_CONNECTION;
                else
                    newConnection.destination = pin->pin;
            }
        }

        draggingConnector->setTooltip({});
        if (initialDraggingConnection == EMPTY_CONNECTION)
            delete draggingConnector;
        else {
            draggingConnector->setInput(initialDraggingConnection.source);
            draggingConnector->setOutput(initialDraggingConnection.destination);
            draggingConnector = nullptr;
        }

        if (newConnection == initialDraggingConnection) {
            initialDraggingConnection = EMPTY_CONNECTION;
            return;
        }

        if (newConnection != EMPTY_CONNECTION)
            graph.addConnection(newConnection);

        if (initialDraggingConnection != EMPTY_CONNECTION) {
            graph.removeConnection(initialDraggingConnection);
            initialDraggingConnection = EMPTY_CONNECTION;
        }
    }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (nodeId == audioInputProcessor->getNodeId()) {
            return audioInputProcessor.get();
        } else if (nodeId == audioOutputProcessor->getNodeId()) {
            return audioOutputProcessor.get();
        } else {
            return tracks->getProcessorForNodeId(nodeId);
        }
    }

    ProcessorGraph &graph;
    Project &project;

private:
    const AudioProcessorGraph::Connection EMPTY_CONNECTION {{0, 0}, {0, 0}};
    std::unique_ptr<GraphEditorConnectors> connectors;
    GraphEditorConnector *draggingConnector {};
    std::unique_ptr<GraphEditorTracks> tracks;
    std::unique_ptr<GraphEditorProcessor> audioInputProcessor;
    std::unique_ptr<GraphEditorProcessor> audioOutputProcessor;
    AudioProcessorGraph::Connection initialDraggingConnection { EMPTY_CONNECTION };

    PinComponent *findPinAt(const MouseEvent &e) const {
        if (auto *pin = audioInputProcessor->findPinAt(e)) {
            return pin;
        } else if ((pin = audioOutputProcessor->findPinAt(e))) {
            return pin;
        }
        return tracks->findPinAt(e);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
