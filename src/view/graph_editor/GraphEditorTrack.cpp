#include "GraphEditorTrack.h"

GraphEditorTrack::GraphEditorTrack(const ValueTree &state, View &view, Tracks &tracks, Project &project, ProcessorGraph &processorGraph, PluginManager &pluginManager,
                                   ConnectorDragListener &connectorDragListener)
        : state(state), view(view), tracks(tracks), project(project), processorGraph(processorGraph), connectorDragListener(connectorDragListener),
          lanes(Tracks::getProcessorLanesForTrack(state), view, tracks, processorGraph, connectorDragListener) {
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

void GraphEditorTrack::resized() {
    auto r = getLocalBounds();

    auto trackInputBounds = isMasterTrack() ?
                            r.removeFromTop(View::TRACK_INPUT_HEIGHT)
                                    .withSize(view.getTrackWidth(), View::TRACK_INPUT_HEIGHT) :
                            r.removeFromTop(View::TRACK_INPUT_HEIGHT);

    const auto &trackOutputBounds = isMasterTrack()
                                    ? r.removeFromRight(tracks.getProcessorSlotSize(getState()))
                                    : r.removeFromBottom(tracks.getProcessorSlotSize(getState()));

    if (trackInputProcessorView != nullptr)
        trackInputProcessorView->setBounds(trackInputBounds);
    if (trackOutputProcessorView != nullptr)
        trackOutputProcessorView->setBounds(trackOutputBounds);
    const auto &laneBounds = isMasterTrack() ? r.reduced(0, 2) : r.reduced(2, 0);
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

void GraphEditorTrack::onColourChanged() {
    const auto &colour = getColour();
    if (trackInputProcessorView != nullptr)
        trackInputProcessorView->setColour(ResizableWindow::backgroundColourId, colour);
    if (trackOutputProcessorView != nullptr)
        trackOutputProcessorView->setColour(ResizableWindow::backgroundColourId, colour);
}

void GraphEditorTrack::trackInputProcessorChanged() {
    const ValueTree &trackInputProcessor = Tracks::getInputProcessorForTrack(state);
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
    const ValueTree &trackOutputProcessor = Tracks::getOutputProcessorForTrack(state);
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
    if (child.hasType(ProcessorIDs::PROCESSOR)) {
        if (child.getProperty(ProcessorIDs::name) == InternalPluginFormat::getTrackInputProcessorName())
            trackInputProcessorChanged();
        else if (child.getProperty(ProcessorIDs::name) == InternalPluginFormat::getTrackOutputProcessorName())
            trackOutputProcessorChanged();
    }
}

void GraphEditorTrack::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (child.hasType(ProcessorIDs::PROCESSOR)) {
        if (child.getProperty(ProcessorIDs::name) == InternalPluginFormat::getTrackInputProcessorName())
            trackInputProcessorChanged();
        else if (child.getProperty(ProcessorIDs::name) == InternalPluginFormat::getTrackOutputProcessorName())
            trackOutputProcessorChanged();
    }
}

void GraphEditorTrack::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ViewIDs::gridViewSlotOffset || ((i == ViewIDs::gridViewTrackOffset || i == ViewIDs::masterViewSlotOffset) && isMasterTrack()))
        resized();

    if (tree != state)
        return;

    if (i == TrackIDs::name) {
        if (trackInputProcessorView != nullptr)
            trackInputProcessorView->setTrackName(tree[TrackIDs::name].toString());
    } else if (i == TrackIDs::colour || i == TrackIDs::selected) {
        onColourChanged();
    }
}
