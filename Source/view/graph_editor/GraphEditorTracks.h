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
        auto r = getLocalBounds().withHeight(getHeight() * Project::NUM_AVAILABLE_PROCESSOR_SLOTS / (Project::NUM_AVAILABLE_PROCESSOR_SLOTS + 1));
        const int w = r.getWidth() / Project::NUM_VISIBLE_TRACKS;

        for (auto *track : objects) {
            if (!track->isMasterTrack()) {
                track->setBounds(r.removeFromLeft(w));
            } else {
                track->setBounds(getLocalBounds().removeFromBottom(getHeight() - r.getHeight()));
            }
        }
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::TRACK) || v.hasType(IDs::MASTER_TRACK);
    }

    GraphEditorTrack *createNewObject(const ValueTree &v) override {
        auto *track = new GraphEditorTrack(project, v, connectorDragListener, graph);
        addAndMakeVisible(track);
        track->addMouseListener(this, true);
        return track;
    }

    void deleteObject(GraphEditorTrack *track) override {
        track->removeMouseListener(this);
        delete track;
    }

    void newObjectAdded(GraphEditorTrack *) override { getParentComponent()->resized(); }

    void objectRemoved(GraphEditorTrack *) override { getParentComponent()->resized(); }

    void objectOrderChanged() override { getParentComponent()->resized(); }

    GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *track : objects) {
            auto *processor = track->getProcessorForNodeId(nodeId);
            if (processor != nullptr) {
                return processor;
            }
        }
        return nullptr;
    }

    GraphEditorTrack *getTrackForState(const ValueTree& state) const {
        for (auto *track : objects) {
            if (track->state == state) {
                return track;
            }
        }

        return nullptr;
    }

    PinComponent *findPinAt(const MouseEvent &e) const {
        for (auto *track : objects) {
            auto *pin = track->findPinAt(e);
            if (pin != nullptr) {
                return pin;
            }
        }
        return nullptr;
    }

    void updateProcessors() {
        resized();
        for (auto *track : objects) {
            track->update();
        }
    }
    
    Point<int> trackAndSlotAt(const MouseEvent &e) {
        for (auto* track : objects) {
            if (track->contains(e.getEventRelativeTo(track).getPosition())) {
                return { track->getTrackIndex(), track->findSlotAt(e) };
            }
        }
        return { -1, -1 };
    }
    
    void mouseDown(const MouseEvent &e) override {
        if (auto* track = dynamic_cast<GraphEditorTrack *>(e.originalComponent->getParentComponent())) {
            if (e.originalComponent == track->getDragControlComponent() && track->state != project.getMasterTrack()) {
                currentlyDraggingTrack = track;
            }
        } else if (auto* processor = dynamic_cast<GraphEditorProcessor *>(e.originalComponent)) {
            if (!e.mods.isRightButtonDown()) {
                currentlyDraggingProcessor = processor;
                auto *processorTrack = dynamic_cast<GraphEditorTrack *>(processor->getParentComponent()->getParentComponent());
                const Point<int> trackAndSlot{processorTrack->getTrackIndex(), processor->getSlot()};
                graph.beginDraggingNode(processor->getNodeId(), trackAndSlot);
            }
        }
    }

    void mouseDrag(const MouseEvent &e) override {
        if (e.originalComponent->getParentComponent() == currentlyDraggingTrack) {
            auto pos = e.getEventRelativeTo(this).getPosition();
            int currentIndex = parent.indexOf(currentlyDraggingTrack->state);
            for (auto* track : objects) {
                if (track == currentlyDraggingTrack)
                    continue;
                if (pos.x < track->getX() + track->getWidth() / 2) {
                    int newIndex = jlimit(0, objects.size() - 1, objects.indexOf(track));
                    if (currentIndex != newIndex) {
                        if (currentIndex < newIndex) {
                            --newIndex;
                        }
                        return parent.moveChild(currentIndex, newIndex, project.getUndoManager());
                    }
                }
            }
        } else if (e.originalComponent == currentlyDraggingProcessor && !e.mods.isRightButtonDown()) {
            const Point<int> &trackAndSlot = trackAndSlotAt(e);
            if (trackAndSlot.x != -1 && trackAndSlot.y != -1) {
                graph.setNodePosition(currentlyDraggingProcessor->getNodeId(), trackAndSlot);
            }
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (currentlyDraggingProcessor != nullptr)
            graph.endDraggingNode(currentlyDraggingProcessor->getNodeId());
        currentlyDraggingTrack = nullptr;
        currentlyDraggingProcessor = nullptr;
    }

    Project& project;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    GraphEditorTrack *currentlyDraggingTrack {};
    GraphEditorProcessor *currentlyDraggingProcessor {};

    void valueTreeChildWillBeMovedToNewParent(ValueTree child, const ValueTree& oldParent, int oldIndex, const ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR)) {
            auto *fromTrack = getTrackForState(oldParent);
            auto *toTrack = getTrackForState(newParent);
            auto *processor = fromTrack->getProcessorForNodeId(ProcessorGraph::getNodeIdForState(child));
            fromTrack->setCurrentlyMovingProcessor(processor);
            toTrack->setCurrentlyMovingProcessor(processor);
        }
    }

    void valueTreeChildHasMovedToNewParent(ValueTree child, const ValueTree& oldParent, int oldIndex, const ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR)) {
            auto *fromTrack = getTrackForState(oldParent);
            auto *toTrack = getTrackForState(newParent);
            fromTrack->setCurrentlyMovingProcessor(nullptr);
            toTrack->setCurrentlyMovingProcessor(nullptr);
        }
    }
};
