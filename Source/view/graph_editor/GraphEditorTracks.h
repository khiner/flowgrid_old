#pragma once

#include <ValueTreeObjectList.h>
#include <state/Project.h>
#include <ProcessorGraph.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"
#include "ConnectorDragListener.h"

class GraphEditorTracks : public Component,
                          private Utilities::ValueTreeObjectList<GraphEditorTrack>,
                          public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTracks(Project& project, TracksState &tracks, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorTrack>(tracks.getState()), project(project),
              tracks(tracks), view(project.getViewStateManager()),
              connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
        view.addListener(this);
    }

    ~GraphEditorTracks() override {
        view.removeListener(this);
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto trackBounds = r.removeFromTop(tracks.getProcessorHeight() * (view.getNumTrackProcessorSlots() + 1) + TracksState::TRACK_LABEL_HEIGHT);
        trackBounds.removeFromLeft(jmax(0, view.getMasterViewSlotOffset() - view.getGridViewTrackOffset()) * tracks.getTrackWidth() + TracksState::TRACK_LABEL_HEIGHT);

        for (auto *track : objects) {
            if (track->isMasterTrack())
                continue;
            track->setBounds(trackBounds.removeFromLeft(tracks.getTrackWidth()));
        }

        if (auto* masterTrack = findMasterTrack()) {
            masterTrack->setBounds(
                    r.removeFromTop(tracks.getProcessorHeight())
                     .withX(tracks.getTrackWidth() * jmax(0, view.getGridViewTrackOffset() - view.getMasterViewSlotOffset()))
                     .withWidth(TracksState::TRACK_LABEL_HEIGHT + tracks.getTrackWidth() * view.getNumMasterProcessorSlots())
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
        auto *track = new GraphEditorTrack(project, tracks, tree, connectorDragListener, graph);
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

    juce::Point<int> trackAndSlotAt(const MouseEvent &e) {
        for (auto* track : objects) {
            if (track->contains(e.getEventRelativeTo(track).getPosition())) {
                if (const auto* trackLabel = dynamic_cast<Label *>(e.originalComponent))
                    return {track->getTrackIndex(), -1}; // initiated at track label
                else
                    return {track->getTrackIndex(), track->findSlotAt(e)};
            }
        }
        return { -1, -1 };
    }
    
    void mouseDown(const MouseEvent &e) override {
        if (!e.mods.isRightButtonDown()) {
            const auto trackAndSlot = trackAndSlotAt(e);
            if (trackAndSlot.x != -1)
                project.beginDragging(trackAndSlot);
        }
    }

    void mouseDrag(const MouseEvent &e) override {
        if (project.isCurrentlyDraggingProcessor() && !e.mods.isRightButtonDown()) {
            const auto& trackAndSlot = trackAndSlotAt(e);
            if (trackAndSlot.x != -1)
                project.dragToPosition(trackAndSlot);
        }
    }

    void mouseUp(const MouseEvent &e) override {
        project.endDraggingProcessor();
    }

private:
    Project& project;
    TracksState& tracks;
    ViewState& view;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

    void valueTreePropertyChanged(ValueTree &tree, const juce::Identifier &i) override {
        if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) {
            resized();
        }
    }

    // TODO I think we can avoid all this nonsense and just destroy and recreate the graph children. Probably not worth the complexity
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
