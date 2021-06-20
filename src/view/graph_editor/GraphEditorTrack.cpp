#include "GraphEditorTrack.h"

GraphEditorTrack::GraphEditorTrack(Track *track, View &view, Project &project, StatefulAudioProcessorWrappers &processorWrappers, PluginManager &pluginManager,
                                   ConnectorDragListener &connectorDragListener)
        : track(track), view(view),  project(project), processorWrappers(processorWrappers), connectorDragListener(connectorDragListener),
          lanes(track->getProcessorLanes(), track, view, processorWrappers, connectorDragListener) {
    addAndMakeVisible(lanes);

    track->addTrackListener(this);
    view.addListener(this);
}

GraphEditorTrack::~GraphEditorTrack() {
    view.removeListener(this);
    track->removeTrackListener(this);
}

void GraphEditorTrack::paint(Graphics &g) {
    if (trackInputProcessorView == nullptr || trackOutputProcessorView == nullptr) return;

    g.setColour(track->getDisplayColour());

    auto inBounds = trackInputProcessorView->getBoxBounds();
    auto outBounds = trackOutputProcessorView->getBoxBounds();
    inBounds.setPosition(inBounds.getPosition() + trackInputProcessorView->getPosition());
    outBounds.setPosition(outBounds.getPosition() + trackOutputProcessorView->getPosition());

    bool isMaster = track->isMaster();
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
    const auto processorSlotSize = view.getProcessorSlotSize(track->isMaster());
    const auto &trackOutputBounds = isMaster() ? r.removeFromRight(processorSlotSize) : r.removeFromBottom(processorSlotSize);

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
        trackInputProcessorView->setColour(ResizableWindow::backgroundColourId, track->getColour());
    if (trackOutputProcessorView != nullptr)
        trackOutputProcessorView->setColour(ResizableWindow::backgroundColourId, track->getColour());
}
