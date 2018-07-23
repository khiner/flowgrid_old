#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"

class GraphEditorTracks : public Component, public Utilities::ValueTreeObjectList<GraphEditorTrack> {
public:
    explicit GraphEditorTracks(const ValueTree &state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorTrack>(state), connectorDragListener(connectorDragListener), graph(graph) {
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
        auto *at = new GraphEditorTrack(v, connectorDragListener, graph);
        addAndMakeVisible(at);
        return at;
    }

    void deleteObject(GraphEditorTrack *at) override {
        delete at;
    }

    void newObjectAdded(GraphEditorTrack *) override { resized(); }

    void objectRemoved(GraphEditorTrack *) override { resized(); }

    void objectOrderChanged() override { resized(); }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
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

    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
};
