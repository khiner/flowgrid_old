#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"

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

    bool isSuitableType(const juce::ValueTree &v) const override {
        return v.hasType(IDs::TRACK) || v.hasType(IDs::MASTER_TRACK);
    }

    GraphEditorTrack *createNewObject(const juce::ValueTree &v) override {
        auto *at = new GraphEditorTrack(project, v, connectorDragListener, graph);
        at->addMouseListener(this, true);
        addAndMakeVisible(at);
        return at;
    }

    void deleteObject(GraphEditorTrack *at) override {
        at->removeMouseListener(this);
        delete at;
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

    PinComponent *findPinAt(const Point<float> &pos) const {
        for (auto *track : objects) {
            auto *processor = track->findPinAt(pos);
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

    void mouseMove (const MouseEvent& event) override {}

    void mouseEnter (const MouseEvent& event) override {}

    void mouseExit (const MouseEvent& event) override {}

    void mouseDown (const MouseEvent& event) override {
        if (auto* processor = dynamic_cast<GraphEditorProcessor *>(event.eventComponent)) {
            currentlyDraggingProcessor = processor;
        }
    }

    void mouseDrag (const MouseEvent& event) override {}

    void mouseUp (const MouseEvent& event) override {
        if (event.eventComponent == currentlyDraggingProcessor) {
            currentlyDraggingProcessor = nullptr;
        }
    }

    void mouseDoubleClick (const MouseEvent& event) override {}

    void mouseWheelMove (const MouseEvent& event, const MouseWheelDetails& wheel) override {}
    void mouseMagnify (const MouseEvent& event, float scaleFactor) override {}
private:
    GraphEditorProcessor *currentlyDraggingProcessor {};
};
