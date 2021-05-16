#pragma once

#include "state/Project.h"
#include "GraphEditorTrack.h"
#include "ConnectorDragListener.h"
#include "ValueTreeObjectList.h"

class GraphEditorTracks : public Component,
                          private ValueTreeObjectList<GraphEditorTrack>,
                          public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTracks(View &view, Tracks &tracks, Project &project, ProcessorGraph &processorGraph, PluginManager &pluginManager, ConnectorDragListener &connectorDragListener)
            : ValueTreeObjectList<GraphEditorTrack>(tracks.getState()),
              view(view), tracks(tracks), project(project), processorGraph(processorGraph), pluginManager(pluginManager), connectorDragListener(connectorDragListener) {
        rebuildObjects();
        view.addListener(this);
    }

    ~GraphEditorTracks() override {
        view.removeListener(this);
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto trackBounds = r.removeFromTop(View::TRACK_LABEL_HEIGHT + view.getProcessorHeight() * (View::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1));
        trackBounds.setWidth(view.getTrackWidth());
        trackBounds.setX(-view.getGridViewTrackOffset() * view.getTrackWidth() + View::TRACKS_MARGIN);

        for (auto *track : objects) {
            if (track->isMasterTrack())
                continue;
            track->setBounds(trackBounds);
            trackBounds.setX(trackBounds.getX() + view.getTrackWidth());
        }

        if (auto *masterTrack = findMasterTrack()) {
            masterTrack->setBounds(r.withX(View::TRACKS_MARGIN).withWidth(view.getTrackWidth() * (View::NUM_VISIBLE_MASTER_TRACK_SLOTS + 1)));
        }
    }

    GraphEditorTrack *findMasterTrack() const {
        for (auto *track : objects)
            if (track->isMasterTrack())
                return track;

        return nullptr;
    }

    bool isSuitableType(const ValueTree &tree) const override {
        return Track::isType(tree);
    }

    GraphEditorTrack *createNewObject(const ValueTree &tree) override {
        auto *track = new GraphEditorTrack(tree, view, tracks, project, processorGraph, pluginManager, connectorDragListener);
        addAndMakeVisible(track);
        return track;
    }

    void deleteObject(GraphEditorTrack *track) override {
        delete track;
    }

    void newObjectAdded(GraphEditorTrack *track) override { resized(); }

    void objectRemoved(GraphEditorTrack *) override { resized(); }

    void objectOrderChanged() override {
        resized();
        connectorDragListener.update();
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *track : objects)
            if (auto *processor = track->getProcessorForNodeId(nodeId))
                return processor;

        return nullptr;
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const {
        for (auto *track : objects)
            if (auto *channel = track->findChannelAt(e))
                return channel;

        return nullptr;
    }

    ValueTree findTrackAt(const juce::Point<int> position) {
        auto *masterTrack = findMasterTrack();
        if (masterTrack != nullptr && masterTrack->getPosition().y <= position.y)
            return masterTrack->getState();

        const auto &nonMasterTrackObjects = getNonMasterTrackObjects();
        for (auto *track : nonMasterTrackObjects)
            if (position.x <= track->getRight())
                return track->getState();

        auto *lastNonMasterTrack = nonMasterTrackObjects.getLast();
        if (lastNonMasterTrack != nullptr)
            return lastNonMasterTrack->getState();

        return {};
    }

private:
    View &view;
    Tracks &tracks;
    Project &project;
    ProcessorGraph &processorGraph;
    PluginManager &pluginManager;
    ConnectorDragListener &connectorDragListener;

    void valueTreePropertyChanged(ValueTree &tree, const juce::Identifier &i) override {
        if (i == ViewIDs::gridViewTrackOffset || i == ViewIDs::masterViewSlotOffset)
            resized();
    }

    Array<GraphEditorTrack *> getNonMasterTrackObjects() {
        Array<GraphEditorTrack *> nonMasterTrackObjects;
        for (auto *trackObject : objects)
            if (!trackObject->isMasterTrack())
                nonMasterTrackObjects.add(trackObject);
        return nonMasterTrackObjects;
    }
};
