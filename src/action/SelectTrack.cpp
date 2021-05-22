#include "SelectTrack.h"

SelectTrack::SelectTrack(const ValueTree &track, bool selected, bool deselectOthers, Tracks &tracks, Connections &connections, View &view, Input &input,
                         ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, processorGraph) {
    if (!track.isValid()) return; // no-op

    auto trackIndex = tracks.indexOf(track);
    // take care of this track
    newTrackSelections.setUnchecked(trackIndex, selected);
    if (selected) {
        newSelectedSlotsMasks.setUnchecked(trackIndex, tracks.createFullSelectionBitmask(track));

        const ValueTree &firstProcessor = Track::getProcessorLane(track).getChild(0);
        setNewFocusedSlot({trackIndex, firstProcessor.isValid() ? Processor::getSlot(firstProcessor) : 0});
    } else {
        newSelectedSlotsMasks.setUnchecked(trackIndex, BigInteger().toString(2));
    }
    // take care of other tracks
    if (selected && deselectOthers) {
        for (int i = 0; i < newTrackSelections.size(); i++) {
            if (i != trackIndex) {
                newTrackSelections.setUnchecked(i, false);
                newSelectedSlotsMasks.setUnchecked(i, BigInteger().toString(2));
            }
        }
    }
}
