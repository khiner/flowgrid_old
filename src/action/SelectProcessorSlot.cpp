#include "SelectProcessorSlot.h"

SelectProcessorSlot::SelectProcessorSlot(const Track *track, int slot, bool selected, bool deselectOthers, Tracks &tracks, Connections &connections, View &view, Input &input,
                                         ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, processorGraph) {
    const auto currentSlotMask = track->getSlotMask();
    if (deselectOthers) {
        for (int i = 0; i < newTrackSelections.size(); i++) {
            newTrackSelections.setUnchecked(i, false);
            newSelectedSlotsMasks.setUnchecked(i, BigInteger());
        }
    }

    auto newSlotMask = deselectOthers ? BigInteger() : currentSlotMask;
    newSlotMask.setBit(slot, selected);
    auto trackIndex = track->getIndex();
    newSelectedSlotsMasks.setUnchecked(trackIndex, newSlotMask);
    if (selected)
        setNewFocusedSlot({trackIndex, slot});
}
