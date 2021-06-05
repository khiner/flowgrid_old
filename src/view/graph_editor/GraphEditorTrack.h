#pragma once

#include "view/graph_editor/processor/TrackInputGraphEditorProcessor.h"
#include "view/graph_editor/processor/TrackOutputGraphEditorProcessor.h"
#include "GraphEditorProcessorLanes.h"

class GraphEditorTrack : public Component, public ValueTree::Listener, public GraphEditorProcessorContainer, private Track::Listener {
public:
    explicit GraphEditorTrack(Track *track, View &view, Project &project, StatefulAudioProcessorWrappers &processorWrappers, PluginManager &pluginManager, ConnectorDragListener &connectorDragListener);

    ~GraphEditorTrack() override;

    Track *getTrack() const { return track; }
    const ValueTree &getState() const { return track->getState(); }

    bool isMaster() const { return track->isMaster(); }

    void paint(Graphics &g) override;
    void resized() override;

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override;
    GraphEditorChannel *findChannelAt(const MouseEvent &e);

private:
    static constexpr int BORDER_WIDTH = 2;

    Track *track;
    View &view;
    Project &project;
    StatefulAudioProcessorWrappers &processorWrappers;
    std::unique_ptr<TrackInputGraphEditorProcessor> trackInputProcessorView;
    std::unique_ptr<TrackOutputGraphEditorProcessor> trackOutputProcessorView;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorLanes lanes;

    void onColourChanged();
    void trackInputProcessorChanged();
    void trackOutputProcessorChanged();

    void processorAdded(Processor *processor) override {
        if (processor->isTrackInputProcessor())
            trackInputProcessorChanged();
        else if (processor->isTrackOutputProcessor())
            trackOutputProcessorChanged();
    }
    void processorRemoved(Processor *processor, int oldIndex) override {
        if (processor->isTrackInputProcessor())
            trackInputProcessorChanged();
        else if (processor->isTrackOutputProcessor())
            trackOutputProcessorChanged();
    }
    void trackPropertyChanged(Track *track, const Identifier &i) override {
        if (i == TrackIDs::name) {
            if (trackInputProcessorView != nullptr)
                // TODO processor should listen to this itself, then remove `setTrackName` method
                trackInputProcessorView->setTrackName(track->getName());
        } else if (i == TrackIDs::colour || i == TrackIDs::selected) {
            onColourChanged();
        }
    }
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == ViewIDs::gridSlotOffset || ((i == ViewIDs::gridTrackOffset || i == ViewIDs::masterSlotOffset) && isMaster())) {
            resized();
        }
    }
};
