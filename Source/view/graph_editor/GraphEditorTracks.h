#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include <ProcessorGraph.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"
#include "ConnectorDragListener.h"

class GraphEditorTracks : public Component,
                          private Utilities::ValueTreeObjectList<GraphEditorTrack>,
                          public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTracks(Project& project, const ValueTree &state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorTrack>(state), project(project),
              tracksManager(project.getTracksManager()), viewManager(project.getViewStateManager()),
              connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
        viewManager.getState().addListener(this);
    }

    ~GraphEditorTracks() override {
        viewManager.getState().removeListener(this);
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto trackBounds = r.removeFromTop(tracksManager.getProcessorHeight() * (viewManager.getNumTrackProcessorSlots() + 1) + TracksStateManager::TRACK_LABEL_HEIGHT);
        trackBounds.removeFromLeft(jmax(0, viewManager.getMasterViewSlotOffset() - viewManager.getGridViewTrackOffset()) * tracksManager.getTrackWidth() + TracksStateManager::TRACK_LABEL_HEIGHT);

        for (auto *track : objects) {
            if (track->isMasterTrack())
                continue;
            track->setBounds(trackBounds.removeFromLeft(tracksManager.getTrackWidth()));
        }

        if (auto* masterTrack = findMasterTrack()) {
            masterTrack->setBounds(
                    r.removeFromTop(tracksManager.getProcessorHeight())
                     .withX(tracksManager.getTrackWidth() * jmax(0, viewManager.getGridViewTrackOffset() - viewManager.getMasterViewSlotOffset()))
                     .withWidth(TracksStateManager::TRACK_LABEL_HEIGHT + tracksManager.getTrackWidth() * viewManager.getNumMasterProcessorSlots())
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

    bool isSuitableType(const ValueTree &tree) const override {
        return tree.hasType(IDs::TRACK);
    }

    GraphEditorTrack *createNewObject(const ValueTree &tree) override {
        auto *track = new GraphEditorTrack(project, tree, connectorDragListener, graph);
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

    GraphEditorProcessor *getProcessorForState(const ValueTree& state) const {
        return getProcessorForNodeId(ProcessorGraph::getNodeIdForState(state));
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
            if (e.originalComponent == track->getDragControlComponent() && track->getState() != tracksManager.getMasterTrack()) {
                project.setCurrentlyDraggingTrack(track->getState());
            }
        } else if (auto* processor = dynamic_cast<GraphEditorProcessor *>(e.originalComponent)) {
            if (!e.mods.isRightButtonDown()) {
                project.beginDraggingProcessor(processor->getState());
            }
        }
    }

    void mouseDrag(const MouseEvent &e) override {
        if (e.originalComponent->getParentComponent() == getTrackForState(project.getCurrentlyDraggingTrack())) {
            auto pos = e.getEventRelativeTo(this).getPosition();
            int currentIndex = parent.indexOf(project.getCurrentlyDraggingTrack());
            for (auto* track : objects) {
                if (track->getState() == project.getCurrentlyDraggingTrack())
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
        } else if (e.originalComponent == getProcessorForState(project.getCurrentlyDraggingProcessor()) && !e.mods.isRightButtonDown()) {
            const Point<int> &trackAndSlot = trackAndSlotAt(e);
            if (trackAndSlot.x != -1 && trackAndSlot.y != -1) {
                project.dragProcessorToPosition(trackAndSlot);
            }
        }
    }

    void mouseUp(const MouseEvent &e) override {
        project.endDraggingProcessor();
    }

private:
    Project& project;
    TracksStateManager& tracksManager;
    ViewStateManager& viewManager;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

    void valueTreePropertyChanged(ValueTree &tree, const juce::Identifier &i) override {
        if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) {
            resized();
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

    void valueTreeChildWillBeMovedToNewParent(ValueTree child, ValueTree& oldParent, int oldIndex, ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR)) {
            auto *fromTrack = getTrackForState(oldParent);
            auto *toTrack = getTrackForState(newParent);
            auto *processor = fromTrack->getProcessorForNodeId(ProcessorGraph::getNodeIdForState(child));
            fromTrack->setCurrentlyMovingProcessor(processor);
            toTrack->setCurrentlyMovingProcessor(processor);
        }
    }

    void valueTreeChildHasMovedToNewParent(ValueTree child, ValueTree& oldParent, int oldIndex, ValueTree& newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR)) {
            auto *fromTrack = getTrackForState(oldParent);
            auto *toTrack = getTrackForState(newParent);
            fromTrack->setCurrentlyMovingProcessor(nullptr);
            toTrack->setCurrentlyMovingProcessor(nullptr);
        }
    }
};
