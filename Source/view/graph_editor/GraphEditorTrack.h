#pragma once

#include <state/Identifiers.h>
#include <view/graph_editor/processor/TrackInputGraphEditorProcessor.h>
#include "view/graph_editor/processor/TrackOutputGraphEditorProcessor.h"
#include "GraphEditorProcessorLanes.h"
#include <Utilities.h>

class GraphEditorTrack : public Component, public ValueTree::Listener, public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTrack(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener);

    ~GraphEditorTrack() override;

    const ValueTree &getState() const { return state; }

    bool isMasterTrack() const { return TracksState::isMasterTrack(state); }

    Colour getColour() const {
        const Colour &trackColour = TracksState::getTrackColour(state);
        return isSelected() ? trackColour.brighter(0.25) : trackColour;
    }

    bool isSelected() const { return state.getProperty(IDs::selected); }

    void paint(Graphics &g) override;

    void resized() override;

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override;

    GraphEditorChannel *findChannelAt(const MouseEvent &e);

private:
    static constexpr int BORDER_WIDTH = 2;

    Project &project;
    TracksState &tracks;
    ViewState &view;
    ValueTree state;

    std::unique_ptr<TrackInputGraphEditorProcessor> trackInputProcessorView;
    std::unique_ptr<TrackOutputGraphEditorProcessor> trackOutputProcessorView;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorLanes lanes;

    void onColourChanged();

    void trackInputProcessorChanged();
    void trackOutputProcessorChanged();

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override;
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
