#pragma once

#include <Utilities.h>
#include <state/Identifiers.h>
#include <view/graph_editor/processor/TrackInputGraphEditorProcessor.h>
#include "JuceHeader.h"
#include "GraphEditorProcessorLane.h"
#include "view/graph_editor/processor/TrackOutputGraphEditorProcessor.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTrack(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : project(project), tracks(tracks), view(project.getView()), state(state),
              connectorDragListener(connectorDragListener), lane(project, TracksState::getProcessorLaneForTrack(state), connectorDragListener) {
        trackBorder.setFill(Colours::transparentBlack);
        trackBorder.setStrokeThickness(2);
        trackBorder.setCornerSize({8, 8});

        addAndMakeVisible(trackBorder);
        addAndMakeVisible(lane);
        trackInputProcessorChanged();
        trackOutputProcessorChanged();

        this->state.addListener(this);
        view.addListener(this);
    }

    ~GraphEditorTrack() override {
        view.removeListener(this);
        this->state.removeListener(this);
    }

    const ValueTree &getState() const { return state; }

    bool isMasterTrack() const { return TracksState::isMasterTrack(state); }

    Colour getColour() const {
        const Colour &trackColour = TracksState::getTrackColour(state);
        return isSelected() ? trackColour.brighter(0.25) : trackColour;
    }

    bool isSelected() const { return state.getProperty(IDs::selected); }

    void resized() override {
        auto r = getLocalBounds();

        const auto &borderBounds = isMasterTrack() ?
                r.toFloat().withTop(ViewState::TRACK_LABEL_HEIGHT - TrackInputGraphEditorProcessor::VERTICAL_MARGIN) :
                r.toFloat().reduced(0, ViewState::TRACKS_MARGIN);
        trackBorder.setRectangle(borderBounds);

        auto trackInputBounds = isMasterTrack() ?
                r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT - TrackInputGraphEditorProcessor::VERTICAL_MARGIN).withWidth(view.getTrackWidth()).withHeight(ViewState::TRACK_LABEL_HEIGHT) :
                r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT);
        const auto &trackOutputBounds = isMasterTrack()
                                        ? r.removeFromRight(lane.getProcessorSlotSize())
                                        : r.removeFromBottom(lane.getProcessorSlotSize());

        if (trackInputProcessorView != nullptr)
            trackInputProcessorView->setBounds(trackInputBounds);
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setBounds(trackOutputBounds);
        const auto &laneBounds = isMasterTrack() ? r.reduced(0, 2) : r.reduced(2, 0);
        lane.setBounds(laneBounds);
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (trackInputProcessorView != nullptr && trackInputProcessorView->getNodeId() == nodeId)
            return trackInputProcessorView.get();
        if (trackOutputProcessorView != nullptr && trackOutputProcessorView->getNodeId() == nodeId)
            return trackOutputProcessorView.get();
        if (auto *currentlyMovingProcessor = lane.getCurrentlyMovingProcessor())
            if (currentlyMovingProcessor->getNodeId() == nodeId)
                return currentlyMovingProcessor;

        return lane.getProcessorForNodeId(nodeId);
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) {
        auto *channel = lane.findChannelAt(e);
        if (channel != nullptr)
            return channel;
        if (trackInputProcessorView != nullptr)
            channel = trackInputProcessorView->findChannelAt(e);
        if (channel != nullptr)
            return channel;
        if (trackOutputProcessorView != nullptr)
            return trackOutputProcessorView->findChannelAt(e);
        return nullptr;
    }

    void setCurrentlyMovingProcessor(BaseGraphEditorProcessor *currentlyMovingProcessor) {
        lane.setCurrentlyMovingProcessor(currentlyMovingProcessor);
    }

private:
    Project &project;
    TracksState &tracks;
    ViewState &view;
    ValueTree state;

    std::unique_ptr<TrackInputGraphEditorProcessor> trackInputProcessorView;
    std::unique_ptr<TrackOutputGraphEditorProcessor> trackOutputProcessorView;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessorLane lane;
    DrawableRectangle trackBorder;

    void onColourChanged() {
        const auto &colour = getColour();
        trackBorder.setStrokeFill(colour);
        if (trackInputProcessorView != nullptr)
            trackInputProcessorView->setColour(ResizableWindow::backgroundColourId, colour);
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setColour(ResizableWindow::backgroundColourId, colour);
    }

    void trackInputProcessorChanged() {
        const ValueTree &trackInputProcessor = TracksState::getInputProcessorForTrack(state);
        if (trackInputProcessor.isValid()) {
            trackInputProcessorView = std::make_unique<TrackInputGraphEditorProcessor>(project, tracks, view, trackInputProcessor, connectorDragListener);
            addAndMakeVisible(trackInputProcessorView.get());
            resized();
        } else {
            removeChildComponent(trackInputProcessorView.get());
            trackInputProcessorView = nullptr;
        }
        onColourChanged();
    }

    void trackOutputProcessorChanged() {
        const ValueTree &trackOutputProcessor = TracksState::getOutputProcessorForTrack(state);
        if (trackOutputProcessor.isValid()) {
            trackOutputProcessorView = std::make_unique<TrackOutputGraphEditorProcessor>(project, tracks, view, trackOutputProcessor, connectorDragListener);
            addAndMakeVisible(trackOutputProcessorView.get());
            resized();
        } else {
            removeChildComponent(trackOutputProcessorView.get());
            trackOutputProcessorView = nullptr;
        }
        onColourChanged();
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (child.getProperty(IDs::name) == TrackInputProcessor::name())
                trackInputProcessorChanged();
            else if (child.getProperty(IDs::name) == TrackOutputProcessor::name())
                trackOutputProcessorChanged();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (child.getProperty(IDs::name) == TrackInputProcessor::name())
                trackInputProcessorChanged();
            else if (child.getProperty(IDs::name) == TrackOutputProcessor::name())
                trackOutputProcessorChanged();
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::gridViewSlotOffset || ((i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) && isMasterTrack()))
            resized();

        if (tree != state)
            return;

        if (i == IDs::name) {
            if (trackInputProcessorView != nullptr)
                trackInputProcessorView->setTrackName(tree[IDs::name].toString());
        } else if (i == IDs::colour || i == IDs::selected) {
            onColourChanged();
        }
    }
};
