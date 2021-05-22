#include "GraphEditorTrack.h"

GraphEditorTrack::GraphEditorTrack(const ValueTree &state, View &view, Tracks &tracks, Project &project, ProcessorGraph &processorGraph, PluginManager &pluginManager,
                                   ConnectorDragListener &connectorDragListener)
        : state(state), view(view), tracks(tracks), project(project), processorGraph(processorGraph), connectorDragListener(connectorDragListener),
          lanes(Track::getProcessorLanes(state), view, tracks, processorGraph, connectorDragListener) {
    addAndMakeVisible(lanes);
    trackInputProcessorChanged();
    trackOutputProcessorChanged();

    this->state.addListener(this);
    view.addListener(this);
}

GraphEditorTrack::~GraphEditorTrack() {
    view.removeListener(this);
    this->state.removeListener(this);
}

void GraphEditorTrack::paint(Graphics &g) {
    if (trackInputProcessorView == nullptr || trackOutputProcessorView == nullptr) return;

    g.setColour(Track::getDisplayColour(state));

    auto inBounds = trackInputProcessorView->getBoxBounds();
    auto outBounds = trackOutputProcessorView->getBoxBounds();
    inBounds.setPosition(inBounds.getPosition() + trackInputProcessorView->getPosition());
    outBounds.setPosition(outBounds.getPosition() + trackOutputProcessorView->getPosition());

    bool isMaster = Track::isMaster(state);
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

void GraphEditorTrack::resized() {
    auto r = getLocalBounds();

    auto trackInputBounds = isMaster() ?
                            r.removeFromTop(View::TRACK_INPUT_HEIGHT)
                                    .withSize(view.getTrackWidth(), View::TRACK_INPUT_HEIGHT) :
                            r.removeFromTop(View::TRACK_INPUT_HEIGHT);

    const auto &trackOutputBounds = isMaster()
                                    ? r.removeFromRight(tracks.getProcessorSlotSize(getState()))
                                    : r.removeFromBottom(tracks.getProcessorSlotSize(getState()));

    if (trackInputProcessorView != nullptr)
        trackInputProcessorView->setBounds(trackInputBounds);
    if (trackOutputProcessorView != nullptr)
        trackOutputProcessorView->setBounds(trackOutputBounds);
    const auto &laneBounds = isMaster() ? r.reduced(0, 2) : r.reduced(2, 0);
    lanes.setBounds(laneBounds);
}

BaseGraphEditorProcessor *GraphEditorTrack::getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
    if (trackInputProcessorView != nullptr && trackInputProcessorView->getNodeId() == nodeId)
        return trackInputProcessorView.get();
    if (trackOutputProcessorView != nullptr && trackOutputProcessorView->getNodeId() == nodeId)
        return trackOutputProcessorView.get();
    return lanes.getProcessorForNodeId(nodeId);
}

GraphEditorChannel *GraphEditorTrack::findChannelAt(const MouseEvent &e) {
    auto *channel = lanes.findChannelAt(e);
    if (channel != nullptr) return channel;

    if (trackInputProcessorView != nullptr)
        channel = trackInputProcessorView->findChannelAt(e);
    if (channel != nullptr) return channel;
    if (trackOutputProcessorView != nullptr) return trackOutputProcessorView->findChannelAt(e);

    return nullptr;
}

void GraphEditorTrack::onColourChanged() {
    if (trackInputProcessorView != nullptr)
        trackInputProcessorView->setColour(ResizableWindow::backgroundColourId, Track::getColour(state));
    if (trackOutputProcessorView != nullptr)
        trackOutputProcessorView->setColour(ResizableWindow::backgroundColourId, Track::getColour(state));
}

void GraphEditorTrack::trackInputProcessorChanged() {
    const ValueTree &trackInputProcessor = Track::getInputProcessor(state);
    if (trackInputProcessor.isValid()) {
        trackInputProcessorView = std::make_unique<TrackInputGraphEditorProcessor>(trackInputProcessor, view, tracks, project, processorGraph, connectorDragListener);
        addAndMakeVisible(trackInputProcessorView.get());
        resized();
    } else {
        removeChildComponent(trackInputProcessorView.get());
        trackInputProcessorView = nullptr;
    }
    onColourChanged();
}

void GraphEditorTrack::trackOutputProcessorChanged() {
    const ValueTree &trackOutputProcessor = Track::getOutputProcessor(state);
    if (trackOutputProcessor.isValid()) {
        trackOutputProcessorView = std::make_unique<TrackOutputGraphEditorProcessor>(trackOutputProcessor, view, tracks, processorGraph, connectorDragListener);
        addAndMakeVisible(trackOutputProcessorView.get());
        resized();
    } else {
        removeChildComponent(trackOutputProcessorView.get());
        trackOutputProcessorView = nullptr;
    }
    onColourChanged();
}

void GraphEditorTrack::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (Processor::isType(child)) {
        if (Processor::isTrackInputProcessor(child))
            trackInputProcessorChanged();
        else if (Processor::isTrackOutputProcessor(child))
            trackOutputProcessorChanged();
    }
}

void GraphEditorTrack::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (Processor::isType(child)) {
        if (Processor::isTrackInputProcessor(child))
            trackInputProcessorChanged();
        else if (Processor::isTrackOutputProcessor(child))
            trackOutputProcessorChanged();
    }
}

void GraphEditorTrack::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ViewIDs::gridViewSlotOffset || ((i == ViewIDs::gridViewTrackOffset || i == ViewIDs::masterViewSlotOffset) && isMaster()))
        resized();

    if (tree != state) return;

    if (i == TrackIDs::name) {
        if (trackInputProcessorView != nullptr)
            // TODO processor should listen to this itself, then remove `ssetTrackName` method
            trackInputProcessorView->setTrackName(Track::getName(tree));
    } else if (i == TrackIDs::colour || i == TrackIDs::selected) {
        onColourChanged();
    }
}
