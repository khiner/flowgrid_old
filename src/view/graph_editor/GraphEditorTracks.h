#pragma once

#include "model/Project.h"
#include "GraphEditorTrack.h"
#include "ConnectorDragListener.h"
#include "model/StatefulList.h"

class GraphEditorTracks : public Component,
                          public GraphEditorProcessorContainer,
                          private ValueTree::Listener,
                          private StatefulList<Track>::Listener {
public:
    explicit GraphEditorTracks(View &view, Tracks &tracks, Project &project, StatefulAudioProcessorWrappers &processorWrappers, PluginManager &pluginManager, ConnectorDragListener &connectorDragListener)
              : view(view), tracks(tracks), project(project), processorWrappers(processorWrappers), pluginManager(pluginManager), connectorDragListener(connectorDragListener) {
        tracks.addChildListener(this);
        view.addStateListener(this);
    }

    ~GraphEditorTracks() override {
        view.removeStateListener(this);
        tracks.removeChildListener(this);
    }

    void resized() override {
        auto r = getLocalBounds();
        auto trackBounds = r.removeFromTop(View::TRACK_LABEL_HEIGHT + view.getProcessorHeight() * (View::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1));
        trackBounds.setWidth(view.getTrackWidth());
        trackBounds.setX(-view.getGridViewTrackOffset() * view.getTrackWidth() + View::TRACKS_MARGIN);

        for (auto *track : children) {
            if (!track->isMaster()) {
                track->setBounds(trackBounds);
                trackBounds.setX(trackBounds.getX() + view.getTrackWidth());
            }
        }

        if (auto *masterTrack = findMasterTrack()) {
            masterTrack->setBounds(r.withX(View::TRACKS_MARGIN).withWidth(view.getTrackWidth() * (View::NUM_VISIBLE_MASTER_TRACK_SLOTS + 1)));
        }
    }

    GraphEditorTrack *findMasterTrack() const {
        for (auto *track : children)
            if (track->isMaster())
                return track;
        return nullptr;
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *track : children)
            if (auto *processor = track->getProcessorForNodeId(nodeId))
                return processor;

        return nullptr;
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const {
        for (auto *track : children)
            if (auto *channel = track->findChannelAt(e))
                return channel;

        return nullptr;
    }

    Track *findTrackAt(const juce::Point<int> position) {
        auto *masterTrack = findMasterTrack();
        if (masterTrack != nullptr && masterTrack->getPosition().y <= position.y)
            return masterTrack->getTrack();

        const auto &nonMasterTrackObjects = getNonMasterTrackObjects();
        for (auto *track : nonMasterTrackObjects)
            if (position.x <= track->getRight())
                return track->getTrack();

        auto *lastNonMasterTrack = nonMasterTrackObjects.getLast();
        if (lastNonMasterTrack != nullptr)
            return lastNonMasterTrack->getTrack();

        return nullptr;
    }

    int compareElements(GraphEditorTrack *first, GraphEditorTrack *second) const {
        return tracks.indexOf(first->getTrack()) - tracks.indexOf(second->getTrack());
    }

private:
    OwnedArray<GraphEditorTrack> children;

    View &view;
    Tracks &tracks;
    Project &project;
    StatefulAudioProcessorWrappers &processorWrappers;
    PluginManager &pluginManager;
    ConnectorDragListener &connectorDragListener;

    void onChildAdded(Track *track) override {
        addAndMakeVisible(children.insert(track->getIndex(), new GraphEditorTrack(track, view, project, processorWrappers, pluginManager, connectorDragListener)));
        resized();
    }
    void onChildRemoved(Track *track, int oldIndex) override {
        children.remove(oldIndex);
        resized();
    }
    void onOrderChanged() override {
        children.sort(*this);
        resized();
        connectorDragListener.update();
    }

    void valueTreePropertyChanged(ValueTree &tree, const juce::Identifier &i) override {
        if (i == ViewIDs::gridTrackOffset || i == ViewIDs::masterSlotOffset)
            resized();
    }

    Array<GraphEditorTrack *> getNonMasterTrackObjects() {
        Array<GraphEditorTrack *> nonMasterTrackObjects;
        for (auto *trackObject : children)
            if (!trackObject->isMaster())
                nonMasterTrackObjects.add(trackObject);
        return nonMasterTrackObjects;
    }
};
