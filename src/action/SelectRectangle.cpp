#include "SelectRectangle.h"

SelectRectangle::SelectRectangle(const juce::Point<int> fromTrackAndSlot, const juce::Point<int> toTrackAndSlot,
                                 Tracks &tracks, Connections &connections, View &view, Input &input, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, allProcessors, processorGraph) {
    Rectangle<int> selectionRectangle(tracks.trackAndSlotToGridPosition(fromTrackAndSlot), tracks.trackAndSlotToGridPosition(toTrackAndSlot));
    selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

    for (int trackIndex = 0; trackIndex < tracks.size(); trackIndex++) {
        auto *track = tracks.getChild(trackIndex);
        int numSlots = view.getNumProcessorSlots(track != nullptr && track->isMaster());
        bool trackSelected = selectionRectangle.contains(tracks.trackAndSlotToGridPosition({trackIndex, -1}));
        newTrackSelections.setUnchecked(trackIndex, trackSelected);
        if (trackSelected) {
            newSelectedSlotsMasks.setUnchecked(trackIndex, Track::createFullSelectionBitmask(numSlots));
        } else {
            BigInteger newSlotsMask;
            for (int otherSlot = 0; otherSlot < numSlots; otherSlot++)
                newSlotsMask.setBit(otherSlot, selectionRectangle.contains(tracks.trackAndSlotToGridPosition({trackIndex, otherSlot})));
            newSelectedSlotsMasks.setUnchecked(trackIndex, newSlotsMask);
        }
    }
    int slotToFocus = toTrackAndSlot.y;
    if (slotToFocus == -1) {
        auto *track = tracks.getChild(toTrackAndSlot.x);
        if (track == nullptr) {
            slotToFocus = 0;
        } else {
            const auto &firstProcessor = track->getFirstProcessorState();
            slotToFocus = firstProcessor.isValid() ? Processor::getSlot(firstProcessor) : 0;
        }
    }
    setNewFocusedSlot({toTrackAndSlot.x, slotToFocus});
}
