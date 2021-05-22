#include "SelectProcessorSlot.h"

SelectProcessorSlot::SelectProcessorSlot(const ValueTree &track, int slot, bool selected, bool deselectOthers, Tracks &tracks, Connections &connections, View &view, Input &input,
                                         ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, processorGraph) {
    const auto currentSlotMask = Track::getSlotMask(track);
    if (deselectOthers) {
        for (int i = 0; i < newTrackSelections.size(); i++) {
            newTrackSelections.setUnchecked(i, false);
            newSelectedSlotsMasks.setUnchecked(i, BigInteger().toString(2));
        }
    }

    auto newSlotMask = deselectOthers ? BigInteger() : currentSlotMask;
    newSlotMask.setBit(slot, selected);
    auto trackIndex = tracks.indexOf(track);
    newSelectedSlotsMasks.setUnchecked(trackIndex, newSlotMask.toString(2));
    if (selected)
        setNewFocusedSlot({trackIndex, slot});
}
