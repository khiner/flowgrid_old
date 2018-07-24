#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include <ProcessorGraph.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"
#include "ConnectorDragListener.h"

class GraphEditorTracks : public Component,
                          public Utilities::ValueTreeObjectList<GraphEditorTrack>,
                          public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTracks(Project& project, const ValueTree &state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorTrack>(state), project(project), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
    }

    ~GraphEditorTracks() override {
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        const int w = r.getWidth() / Project::NUM_VISIBLE_TRACKS;

        for (auto *at : objects)
            at->setBounds(r.removeFromLeft(w));
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::TRACK) || v.hasType(IDs::MASTER_TRACK);
    }

    GraphEditorTrack *createNewObject(const ValueTree &v) override {
        auto *track = new GraphEditorTrack(project, v, connectorDragListener, graph);
        track->addMouseListener(this, true);
        addAndMakeVisible(track);
        return track;
    }

    void deleteObject(GraphEditorTrack *track) override {
        track->removeMouseListener(this);
        delete track;
    }

    void newObjectAdded(GraphEditorTrack *) override { resized(); }

    void objectRemoved(GraphEditorTrack *) override { resized(); }

    void objectOrderChanged() override { resized(); }

    GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        if (currentlyDraggingProcessor != nullptr && currentlyDraggingProcessor->getNodeId() == nodeId)
            return currentlyDraggingProcessor;

        for (auto *track : objects) {
            auto *processor = track->getProcessorForNodeId(nodeId);
            if (processor != nullptr) {
                return processor;
            }
        }
        return nullptr;
    }

    PinComponent *findPinAt(const MouseEvent &e) const {
        for (auto *track : objects) {
            auto *processor = track->findPinAt(e);
            if (processor != nullptr) {
                return processor;
            }
        }
        return nullptr;
    }

    void updateNodes() {
        for (auto *track : objects) {
            track->updateNodes();
        }
    }

    Project& project;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

    void mouseDown (const MouseEvent& event) override {
        if (auto* processor = dynamic_cast<GraphEditorProcessor *>(event.eventComponent)) {
            currentlyDraggingProcessor = processor;
        }
    }

    void mouseUp (const MouseEvent& event) override {
        if (event.eventComponent == currentlyDraggingProcessor) {
            currentlyDraggingProcessor = nullptr;
        }
    }
private:
    GraphEditorProcessor *currentlyDraggingProcessor {};
};
