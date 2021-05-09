#include "GraphEditorProcessorLane.h"

#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "view/graph_editor/processor/ParameterPanelGraphEditorProcessor.h"

GraphEditorProcessorLane::GraphEditorProcessorLane(Project &project, const ValueTree &state, ConnectorDragListener &connectorDragListener)
        : Utilities::ValueTreeObjectList<BaseGraphEditorProcessor>(state),
          project(project), state(state), tracks(project.getTracks()), view(project.getView()),
          connections(project.getConnections()),
          pluginManager(project.getPluginManager()), connectorDragListener(connectorDragListener) {
    rebuildObjects();
    view.addListener(this);
    // TODO shouldn't need to do this
    valueTreePropertyChanged(view.getState(), isMasterTrack() ? IDs::numMasterProcessorSlots : IDs::numProcessorSlots);
    addAndMakeVisible(laneDragRectangle);
    setInterceptsMouseClicks(false, false);
}

GraphEditorProcessorLane::~GraphEditorProcessorLane() {
    view.removeListener(this);
    freeObjects();
}

void GraphEditorProcessorLane::resized() {
    auto slotOffset = getSlotOffset();
    auto processorSlotSize = view.getProcessorSlotSize(getTrack());
    auto r = getLocalBounds();
    if (isMasterTrack()) {
        r.setWidth(processorSlotSize);
        r.setX(-slotOffset * processorSlotSize - ViewState::TRACK_LABEL_HEIGHT);
    } else {
        auto laneHeaderBounds = r.removeFromTop(ViewState::LANE_HEADER_HEIGHT).reduced(0, 2);
        laneDragRectangle.setRectangle(laneHeaderBounds.toFloat());

        r.setHeight(processorSlotSize);
        r.setY(r.getY() - slotOffset * processorSlotSize - ViewState::TRACK_LABEL_HEIGHT);
    }

    for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
        if (slot == slotOffset) {
            if (isMasterTrack())
                r.setX(r.getX() + ViewState::TRACK_LABEL_HEIGHT);
            else
                r.setY(r.getY() + ViewState::TRACK_LABEL_HEIGHT);
        } else if (slot == slotOffset + ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (isMasterTrack() ? 1 : 0)) {
            if (isMasterTrack())
                r.setX(r.getRight());
            else
                r.setY(r.getBottom());
        }
        processorSlotRectangles.getUnchecked(slot)->setRectangle(r.reduced(1).toFloat());
        if (auto *processor = findProcessorAtSlot(slot))
            processor->setBounds(r);
        if (isMasterTrack())
            r.setX(r.getRight());
        else
            r.setY(r.getBottom());
    }
}

void GraphEditorProcessorLane::updateProcessorSlotColours() {
    const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4f);
    const auto &track = getTrack();

    laneDragRectangle.setFill(TracksState::getTrackColour(track));
    for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
        auto fillColour = baseColour;
        if (TracksState::doesTrackHaveSelections(track))
            fillColour = fillColour.brighter(0.2f);
        if (TracksState::isSlotSelected(track, slot))
            fillColour = TracksState::getTrackColour(track);
        if (tracks.isSlotFocused(track, slot))
            fillColour = fillColour.brighter(0.16f);
        if (!view.isProcessorSlotInView(track, slot))
            fillColour = fillColour.darker(0.3f);
        processorSlotRectangles.getUnchecked(slot)->setFill(fillColour);
    }
}

BaseGraphEditorProcessor *GraphEditorProcessorLane::createEditorForProcessor(const ValueTree &processor) {
    if (processor[IDs::name] == MixerChannelProcessor::name()) {
        return new ParameterPanelGraphEditorProcessor(project, tracks, view, processor, connectorDragListener);
    }
    return new LabelGraphEditorProcessor(project, tracks, view, processor, connectorDragListener);
}

BaseGraphEditorProcessor *GraphEditorProcessorLane::createNewObject(const ValueTree &processor) {
    auto *graphEditorProcessor = createEditorForProcessor(processor);
    addAndMakeVisible(graphEditorProcessor);
    return graphEditorProcessor;
}

// TODO only instantiate 64 slot rects (and maybe another set for the boundary perimeter)
//  might be an over-early optimization though
void GraphEditorProcessorLane::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (isSuitableType(tree) && i == IDs::processorSlot) {
        resized();
    } else if (i == IDs::selected || i == IDs::colour ||
               i == IDs::selectedSlotsMask || i == IDs::focusedTrackIndex || i == IDs::focusedProcessorSlot ||
               i == IDs::gridViewTrackOffset) {
        updateProcessorSlotColours();
    } else if (i == IDs::gridViewSlotOffset || (i == IDs::masterViewSlotOffset && isMasterTrack())) {
        resized();
        updateProcessorSlotColours();
    } else if (i == IDs::numProcessorSlots || (i == IDs::numMasterProcessorSlots && isMasterTrack())) {
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
    Utilities::ValueTreeObjectList<BaseGraphEditorProcessor>::valueTreePropertyChanged(tree, i);
}

void GraphEditorProcessorLane::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    ValueTreeObjectList::valueTreeChildAdded(parent, child);
    if (this->parent == parent && isSuitableType(child))
        resized();
}
