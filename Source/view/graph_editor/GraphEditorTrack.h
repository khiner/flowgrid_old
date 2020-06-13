#pragma once

#include <Utilities.h>
#include <state/Identifiers.h>
#include <view/graph_editor/processor/TrackInputGraphEditorProcessor.h>
#include "JuceHeader.h"
#include "GraphEditorProcessorLanes.h"
#include "view/graph_editor/processor/TrackOutputGraphEditorProcessor.h"

class GraphEditorTrack : public Component, public Utilities::ValueTreePropertyChangeListener, public GraphEditorProcessorContainer {
public:
    explicit GraphEditorTrack(Project &project, TracksState &tracks, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : project(project), tracks(tracks), view(project.getView()), state(state),
              connectorDragListener(connectorDragListener), lanes(project, TracksState::getProcessorLanesForTrack(state), connectorDragListener) {
        addAndMakeVisible(lanes);
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

    void paint(Graphics &g) override {
        if (trackInputProcessorView == nullptr || trackOutputProcessorView == nullptr)
            return;

        const auto &colour = getColour();
        g.setColour(colour);

        auto inBounds = trackInputProcessorView->getBoxBounds();
        auto outBounds = trackOutputProcessorView->getBoxBounds();
        inBounds.setPosition(inBounds.getPosition() + trackInputProcessorView->getPosition());
        outBounds.setPosition(outBounds.getPosition() + trackOutputProcessorView->getPosition());

        bool isMaster = isMasterTrack();
        float x = inBounds.getX();
        float y = isMaster ? outBounds.getY() : inBounds.getY();
        float width = isMaster ? outBounds.getRight() - inBounds.getX() : outBounds.getWidth();
        float height = isMaster ? outBounds.getHeight() : outBounds.getBottom() - inBounds.getY();
        bool curveTopLeft = !isMaster, curveTopRight = true, curveBottomLeft = true, curveBottomRight = true;
        Path p;
        p.addRoundedRectangle(x, y, width, height,
                              4.0f, 4.0f, curveTopLeft, curveTopRight, curveBottomLeft, curveBottomRight);
        g.strokePath(p, PathStrokeType(BORDER_WIDTH));
    }

    void resized() override {
        auto r = getLocalBounds();

        auto trackInputBounds = isMasterTrack() ?
                r.removeFromTop(ViewState::TRACK_LABEL_HEIGHT)
                        .withSize(view.getTrackWidth(), ViewState::TRACK_LABEL_HEIGHT) :
                r.removeFromTop(ViewState::TRACK_INPUT_HEIGHT);

        const auto &trackOutputBounds = isMasterTrack()
                                        ? r.removeFromRight(view.getProcessorSlotSize(getState()))
                                        : r.removeFromBottom(view.getProcessorSlotSize(getState()));

        if (trackInputProcessorView != nullptr)
            trackInputProcessorView->setBounds(trackInputBounds);
        if (trackOutputProcessorView != nullptr)
            trackOutputProcessorView->setBounds(trackOutputBounds);
        const auto &laneBounds = isMasterTrack() ? r.reduced(0, 2) : r.reduced(2, 0);
        lanes.setBounds(laneBounds);
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (trackInputProcessorView != nullptr && trackInputProcessorView->getNodeId() == nodeId)
            return trackInputProcessorView.get();
        if (trackOutputProcessorView != nullptr && trackOutputProcessorView->getNodeId() == nodeId)
            return trackOutputProcessorView.get();
        return lanes.getProcessorForNodeId(nodeId);
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) {
        auto *channel = lanes.findChannelAt(e);
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

    void onColourChanged() {
        const auto &colour = getColour();
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
