#include "GraphEditorProcessorLane.h"

#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "view/graph_editor/processor/ParameterPanelGraphEditorProcessor.h"

GraphEditorProcessorLane::GraphEditorProcessorLane(ProcessorLane *lane, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener)
        : lane(lane), track(track), view(view), processorWrappers(processorWrappers), connectorDragListener(connectorDragListener) {
    view.addStateListener(this);
    lane->addChildListener(this);
    // TODO shouldn't need to do this
    valueTreePropertyChanged(view.getState(), track->isMaster() ? ViewIDs::numMasterProcessorSlots : ViewIDs::numProcessorSlots);
    addAndMakeVisible(laneDragRectangle);
    setInterceptsMouseClicks(false, false);
}

GraphEditorProcessorLane::~GraphEditorProcessorLane() {
    lane->removeChildListener(this);
    view.removeStateListener(this);
}

void GraphEditorProcessorLane::resized() {
    bool isMaster = track->isMaster();
    auto slotOffset = getSlotOffset();
    auto processorSlotSize = view.getProcessorSlotSize(isMaster);
    auto r = getLocalBounds();
    if (isMaster) {
        r.setWidth(processorSlotSize);
        r.setX(-slotOffset * processorSlotSize - View::TRACK_LABEL_HEIGHT);
    } else {
        auto laneHeaderBounds = r.removeFromTop(View::LANE_HEADER_HEIGHT).reduced(0, 2);
        laneDragRectangle.setRectangle(laneHeaderBounds.toFloat());

        r.setHeight(processorSlotSize);
        r.setY(r.getY() - slotOffset * processorSlotSize - View::TRACK_LABEL_HEIGHT);
    }

    for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
        if (slot == slotOffset) {
            if (isMaster)
                r.setX(r.getX() + View::TRACK_LABEL_HEIGHT);
            else
                r.setY(r.getY() + View::TRACK_LABEL_HEIGHT);
        } else if (slot == slotOffset + View::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (isMaster ? 1 : 0)) {
            if (isMaster)
                r.setX(r.getRight());
            else
                r.setY(r.getBottom());
        }
        processorSlotRectangles.getUnchecked(slot)->setRectangle(r.reduced(1).toFloat());
        if (auto *processor = findProcessorAtSlot(slot))
            processor->setBounds(r);
        if (isMaster)
            r.setX(r.getRight());
        else
            r.setY(r.getBottom());
    }
}

void GraphEditorProcessorLane::updateProcessorSlotColours() {
    const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4f);
    const auto &track = getTrack();

    laneDragRectangle.setFill(track->getColour());
    for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
        // TODO should be method on track when it has a `view`
        auto fillColour = baseColour;
        if (track->hasSelections())
            fillColour = fillColour.brighter(0.2f);
        if (track->isSlotSelected(slot))
            fillColour = track->getColour();
        auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        if (track->getIndex() == focusedTrackAndSlot.x && slot == focusedTrackAndSlot.y)
            fillColour = fillColour.brighter(0.16f);
        if (!view.isProcessorSlotInView(track->getIndex(), track->isMaster(), slot))
            fillColour = fillColour.darker(0.3f);
        processorSlotRectangles.getUnchecked(slot)->setFill(fillColour);
    }
}

BaseGraphEditorProcessor *GraphEditorProcessorLane::createEditorForProcessor(Processor *processor) {
    if (processor->getName() == InternalPluginFormat::getMixerChannelProcessorName()) {
        return new ParameterPanelGraphEditorProcessor(processor, track, view, processorWrappers, connectorDragListener);
    }
    return new LabelGraphEditorProcessor(processor, track, view, processorWrappers, connectorDragListener);
}

// TODO only instantiate 64 slot rects (and maybe another set for the boundary perimeter)
//  might be an over-early optimization though
void GraphEditorProcessorLane::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    bool isMaster = track->isMaster();
    if (i == TrackIDs::selected || i == TrackIDs::colour ||
               i == ProcessorLaneIDs::selectedSlotsMask || i == ViewIDs::focusedTrackIndex || i == ViewIDs::focusedProcessorSlot ||
               i == ViewIDs::gridTrackOffset) {
        updateProcessorSlotColours();
    } else if (i == ViewIDs::gridSlotOffset || (i == ViewIDs::masterSlotOffset && isMaster)) {
        resized();
        updateProcessorSlotColours();
    } else if (i == ViewIDs::numProcessorSlots || (i == ViewIDs::numMasterProcessorSlots && isMaster)) {
        auto numSlots = getNumSlots();
        while (processorSlotRectangles.size() < numSlots) {
            auto *rect = new DrawableRectangle();
            processorSlotRectangles.add(rect);
            addAndMakeVisible(rect);
            rect->setCornerSize({3, 3});
            rect->toBack();
        }
        processorSlotRectangles.removeLast(processorSlotRectangles.size() - numSlots, true);
        resized();
        updateProcessorSlotColours();
    }
}
