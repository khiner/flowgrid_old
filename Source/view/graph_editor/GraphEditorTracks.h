#pragma once

#include <ValueTreeObjectList.h>
#include <state/Project.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"
#include "ConnectorDragListener.h"

class GraphEditorTracks : public Component,
                          private Utilities::ValueTreeObjectList<GraphEditorTrack>,
                          public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTracks(Project &project, TracksState &tracks, ConnectorDragListener &connectorDragListener)
            : Utilities::ValueTreeObjectList<GraphEditorTrack>(tracks.getState()), project(project),
              tracks(tracks), view(project.getView()),
              connectorDragListener(connectorDragListener) {
        rebuildObjects();
        view.addListener(this);
    }

    ~GraphEditorTracks() override {
        view.removeListener(this);
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto trackBounds = r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT +  view.getProcessorHeight() * ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + ViewState::TRACK_OUTPUT_HEIGHT);
        trackBounds.setWidth(view.getTrackWidth());
        trackBounds.setX(- view.getGridViewTrackOffset() * view.getTrackWidth() + ViewState::TRACK_LABEL_HEIGHT);

        for (auto *track : objects) {
            if (track->isMasterTrack())
                continue;
            track->setBounds(trackBounds);
            trackBounds.setX(trackBounds.getX() + view.getTrackWidth());
        }

        if (auto *masterTrack = findMasterTrack()) {
            masterTrack->setBounds(
                    r.removeFromTop(view.getProcessorHeight())
                            .withWidth(ViewState::TRACK_LABEL_HEIGHT + view.getTrackWidth() * view.getNumMasterProcessorSlots() + ViewState::TRACK_OUTPUT_HEIGHT)
            );
        }
    }

    GraphEditorTrack *findMasterTrack() const {
        for (auto *track : objects)
            if (track->isMasterTrack())
                return track;

        return nullptr;
    }

    bool isSuitableType(const ValueTree &tree) const override {
        return tree.hasType(IDs::TRACK);
    }

    GraphEditorTrack *createNewObject(const ValueTree &tree) override {
        auto *track = new GraphEditorTrack(project, tracks, tree, connectorDragListener);
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
        for (auto *track : objects)
            if (auto *processor = track->getProcessorForNodeId(nodeId))
                return processor;

        return nullptr;
    }

    GraphEditorTrack *getTrackForState(const ValueTree &state) const {
        for (auto *track : objects)
            if (track->getState() == state)
                return track;

        return nullptr;
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        for (auto *track : objects)
            if (auto *pin = track->findPinAt(e))
                return pin;

        return nullptr;
    }

private:
    Project &project;
    TracksState &tracks;
    ViewState &view;
    ConnectorDragListener &connectorDragListener;


    void valueTreePropertyChanged(ValueTree &tree, const juce::Identifier &i) override {
        if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset)
            resized();
    }

    // TODO I think we can avoid all this nonsense and just destroy and recreate the graph children. Probably not worth the complexity
    void valueTreeChildWillBeMovedToNewParent(ValueTree child, ValueTree &oldParent, int oldIndex, ValueTree &newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR)) {
            auto *fromTrack = getTrackForState(oldParent);
            auto *toTrack = getTrackForState(newParent);
            auto *processor = fromTrack->getProcessorForNodeId(ProcessorGraph::getNodeIdForState(child));
            fromTrack->setCurrentlyMovingProcessor(processor);
            toTrack->setCurrentlyMovingProcessor(processor);
        }
    }

    void valueTreeChildHasMovedToNewParent(ValueTree child, ValueTree &oldParent, int oldIndex, ValueTree &newParent, int newIndex) override {
        if (child.hasType(IDs::PROCESSOR)) {
            auto *fromTrack = getTrackForState(oldParent);
            auto *toTrack = getTrackForState(newParent);
            fromTrack->setCurrentlyMovingProcessor(nullptr);
            toTrack->setCurrentlyMovingProcessor(nullptr);
        }
    }
};
