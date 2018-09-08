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
        project.getViewState().addListener(this);
    }

    ~GraphEditorTracks() override {
        project.getViewState().removeListener(this);
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto trackBounds = r.removeFromTop(project.getProcessorHeight() * (project.getNumTrackProcessorSlots() + 1) + Project::TRACK_LABEL_HEIGHT);
        trackBounds.removeFromLeft(jmax(0, project.getMasterViewSlotOffset() - project.getGridViewTrackOffset()) * project.getTrackWidth() + Project::TRACK_LABEL_HEIGHT);

        for (auto *track : objects) {
            if (track->isMasterTrack())
                continue;
            track->setBounds(trackBounds.removeFromLeft(project.getTrackWidth()));
        }

        if (auto* masterTrack = findMasterTrack()) {
            masterTrack->setBounds(
                    r.removeFromTop(project.getProcessorHeight())
                     .withX(project.getTrackWidth() * jmax(0, project.getGridViewTrackOffset() - project.getMasterViewSlotOffset()))
                     .withWidth(Project::TRACK_LABEL_HEIGHT + project.getTrackWidth() * project.getNumMasterProcessorSlots())
            );
        }
    }

    GraphEditorTrack* findMasterTrack() const {
        for (auto *track : objects) {
            if (track->isMasterTrack())
                return track;
        }
        return nullptr;
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::TRACK);
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

    void newObjectAdded(GraphEditorTrack *track) override { resized(); }

    void objectRemoved(GraphEditorTrack *) override { resized(); }

    void objectOrderChanged() override {
        resized();
        connectorDragListener.update();
    }

    GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *track : objects) {
            auto *processor = track->getProcessorForNodeId(nodeId);
            if (processor != nullptr)
                return processor;
        }
        return nullptr;
    }

    GraphEditorTrack *getTrackForState(const ValueTree& state) const {
        for (auto *track : objects) {
            if (track->getState() == state)
                return track;
        }
        return nullptr;
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        for (auto *track : objects) {
            auto *pin = track->findPinAt(e);
            if (pin != nullptr)
                return pin;
        }
        return nullptr;
    }

    Point<int> trackAndSlotAt(const MouseEvent &e) {
        for (auto* track : objects) {
            if (track->contains(e.getEventRelativeTo(track).getPosition()))
                return { track->getTrackIndex(), track->findSlotAt(e) };
        }
        return { -1, -1 };
    }
    
    void mouseDown(const MouseEvent &e) override {
        if (auto* track = dynamic_cast<GraphEditorTrack *>(e.originalComponent->getParentComponent())) {
            if (e.originalComponent == track->getDragControlComponent() && track->getState() != project.getMasterTrack()) {
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
            int currentIndex = parent.indexOf(currentlyDraggingTrack->getState());
            for (auto* track : objects) {
                if (track == currentlyDraggingTrack)
                    continue;
                if (pos.x < track->getX() + track->getWidth() / 2) {
                    int newIndex = jlimit(0, objects.size() - 1, objects.indexOf(track));
                    if (currentIndex != newIndex) {
                        if (currentIndex < newIndex) {
                            --newIndex;
                        }
                        return parent.moveChild(currentIndex, newIndex, &project.getUndoManager());
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

    void valueTreePropertyChanged(ValueTree &v, const juce::Identifier &i) override {
        if (i == IDs::selected && v[IDs::selected]) {
            if (isSuitableType(v)) {
                deselectAllTracksExcept(v);
            } else if (isSuitableType(v.getParent())) {
                deselectAllTracksExcept(v.getParent());
            }
        } else if (v.hasType(IDs::PROCESSOR) && i == IDs::processorSlot) {
            resized();
        } else if (i == IDs::numMasterProcessorSlots || i == IDs::numProcessorSlots) {
        } else if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) {
            resized();
        } else if (i == IDs::gridViewSlotOffset) {
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int index) override {
        ValueTreeObjectList::valueTreeChildRemoved(exParent, tree, index);
        if (parent == exParent && isSuitableType(tree)) {
            auto newSelectedTrackIndex = (index - 1 >= 0) ? index - 1 : index;
            if (newSelectedTrackIndex < objects.size())
                objects.getUnchecked(newSelectedTrackIndex)->setSelected(true);
        }
    }

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

private:
    void deselectAllTracksExcept(const ValueTree& except) {
        for (auto track : parent) {
            if (track != except) {
                if (track[IDs::selected])
                    track.setProperty(IDs::selected, false, nullptr);
                else
                    track.sendPropertyChangeMessage(IDs::selected);
            }
        }
    }
};
