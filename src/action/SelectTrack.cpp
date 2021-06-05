#include "SelectTrack.h"

SelectTrack::SelectTrack(const Track *track, bool selected, bool deselectOthers, Tracks &tracks, Connections &connections, View &view, Input &input,
                         ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, processorGraph) {
    if (track == nullptr) return; // no-op

    auto trackIndex = track->getIndex();
    // take care of this track
    newTrackSelections.setUnchecked(trackIndex, selected);
    if (selected) {
        auto fullSelectionBitmask = Track::createFullSelectionBitmask(view.getNumProcessorSlots(track->isMaster()));
        newSelectedSlotsMasks.setUnchecked(trackIndex, fullSelectionBitmask);

        const auto &firstProcessor = track->getFirstProcessorState();
        setNewFocusedSlot({trackIndex, firstProcessor.isValid() ? Processor::getSlot(firstProcessor) : 0});
    } else {
        newSelectedSlotsMasks.setUnchecked(trackIndex, BigInteger());
    }
    // take care of other tracks
    if (selected && deselectOthers) {
        for (int i = 0; i < newTrackSelections.size(); i++) {
            if (i != trackIndex) {
                newTrackSelections.setUnchecked(i, false);
                newSelectedSlotsMasks.setUnchecked(i, BigInteger());
            }
        }
    }
}
