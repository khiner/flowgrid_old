#pragma once

#include <ProcessorGraph.h>
#include "PinComponent.h"
#include "ProcessorComponent.h"
#include "ConnectorComponent.h"
#include "GraphEditorTracks.h"

class GraphEditorPanel : public Component, public ChangeListener, public ConnectorDragListener {
public:
    GraphEditorPanel(ProcessorGraph &g, Project &project)
            : graph(g), project(project) {
        graph.addChangeListener(this);
        setOpaque(true);

        addAndMakeVisible(*(tracks = std::make_unique<GraphEditorTracks>(project.getTracks())));
    }

    ~GraphEditorPanel() override {
        graph.removeChangeListener(this);
        draggingConnector = nullptr;
        nodes.clear();
        connectors.clear();
    }

    void paint(Graphics &g) override {
        const Colour &bgColor = findColour(ResizableWindow::backgroundColourId);
        g.fillAll(bgColor);
    }

    void mouseDown(const MouseEvent &e) override {
        if (e.mods.isPopupMenu())
            showPopupMenu(e.position.toInt());
    }

    void mouseUp(const MouseEvent &) override {
    }

    void mouseDrag(const MouseEvent &e) override {
    }

    void resized() override {
        auto r = getLocalBounds();
        tracks->setBounds(r.withHeight(int(getHeight() * 8.0 / 9.0)));
        updateComponents();
    }

    void changeListenerCallback(ChangeBroadcaster *) override {
        updateComponents();
    }

    void updateComponents() {
        repaint();
        for (int i = nodes.size(); --i >= 0;)
            if (graph.getNodeForId(nodes.getUnchecked(i)->nodeId) == nullptr)
                nodes.remove(i);

        for (int i = connectors.size(); --i >= 0;)
            if (!graph.isConnectedUi(connectors.getUnchecked(i)->connection))
                connectors.remove(i);

        for (auto *fc : nodes)
            fc->update();

        for (auto *cc : connectors)
            cc->update();

        for (auto *node : graph.getNodes()) {
            ProcessorComponent *component = getComponentForNodeId(node->nodeID);
            if (component == nullptr) {
                auto *comp = nodes.add(new ProcessorComponent(*this, graph, node->nodeID));
                addAndMakeVisible(comp);
                comp->update();
            } else {
                component->repaint();
            }
        }

        for (auto &c : graph.getConnectionsUi()) {
            if (getComponentForConnection(c) == nullptr) {
                auto *comp = connectors.add(new ConnectorComponent(*this, graph));
                addAndMakeVisible(comp);

                comp->setInput(c.source, getComponentForNodeId(c.source.nodeID));
                comp->setOutput(c.destination, getComponentForNodeId(c.destination.nodeID));
            }
        }
    }

    void showPopupMenu(const Point<int> &mousePos) {
        const auto &gridLocation = graph.positionToGridLocation(mousePos.toDouble() / juce::Point<double>((double) getWidth(), (double) getHeight()));
        ValueTree track = project.getTrack(gridLocation.x);
        int slot = gridLocation.y;

        menu = std::make_unique<PopupMenu>();
        project.addPluginsToMenu(*menu, track);
        menu->showMenuAsync({}, ModalCallbackFunction::create([this, track, slot](int r) {
            if (auto *description = project.getChosenType(r)) {
                project.createAndAddProcessor(*description, track, slot);
            }
        }));
    }

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override {
        auto *c = dynamic_cast<ConnectorComponent *> (e.originalComponent);
        connectors.removeObject(c, false);
        draggingConnector.reset(c);

        if (draggingConnector == nullptr)
            draggingConnector = std::make_unique<ConnectorComponent>(*this, graph);

        draggingConnector->setInput(source, getComponentForNodeId(source.nodeID));
        draggingConnector->setOutput(destination, getComponentForNodeId(destination.nodeID));

        addAndMakeVisible(draggingConnector.get());
        draggingConnector->toFront(false);

        dragConnector(e);
    }

    void dragConnector(const MouseEvent &e) override {
        auto e2 = e.getEventRelativeTo(this);

        if (draggingConnector != nullptr) {
            draggingConnector->setTooltip({});

            auto pos = e2.position;

            if (auto *pin = findPinAt(pos)) {
                auto connection = draggingConnector->connection;

                if (connection.source.nodeID == 0 && !pin->isInput) {
                    connection.source = pin->pin;
                } else if (connection.destination.nodeID == 0 && pin->isInput) {
                    connection.destination = pin->pin;
                }

                if (graph.canConnect(connection)) {
                    pos = (pin->getParentComponent()->getPosition() + pin->getBounds().getCentre()).toFloat();
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

        auto e2 = e.getEventRelativeTo(this);
        auto connection = draggingConnector->connection;

        draggingConnector = nullptr;

        if (auto *pin = findPinAt(e2.position)) {
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

    static const int NUM_ROWS = Project::NUM_VISIBLE_PROCESSOR_SLOTS;
    static const int NUM_COLUMNS = Project::NUM_VISIBLE_TRACKS;

private:
    OwnedArray<ProcessorComponent> nodes;
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<PopupMenu> menu;
    std::unique_ptr<GraphEditorTracks> tracks;

    ProcessorComponent *getComponentForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *fc : nodes)
            if (fc->nodeId == nodeId)
                return fc;

        return nullptr;
    }

    ConnectorComponent *getComponentForConnection(const AudioProcessorGraph::Connection &conn) const {
        for (auto *cc : connectors)
            if (cc->connection == conn)
                return cc;

        return nullptr;
    }

    PinComponent *findPinAt(const Point<float> &pos) const {
        for (auto *fc : nodes) {
            // NB: A Visual Studio optimiser error means we have to put this Component* in a local
            // variable before trying to cast it, or it gets mysteriously optimised away..
            auto *comp = fc->getComponentAt(pos.toInt() - fc->getPosition());

            if (auto *pin = dynamic_cast<PinComponent *> (comp))
                return pin;
        }

        return nullptr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
