#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"

struct SelectTrackAction : public SelectAction {
    SelectTrackAction(const ValueTree& track, bool selected, bool deselectOthers,
                      TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager)
            : SelectAction(tracksManager, connectionsManager, viewManager) {
        jassert(track.isValid());

        auto trackIndex = tracksManager.indexOf(track);
        // take care of this track
        newTrackSelections.setUnchecked(trackIndex, selected);
        if (selected) {
            BigInteger selectedSlotsMask;
            selectedSlotsMask.setRange(0, viewManager.getNumAvailableSlotsForTrack(track), true);
            newSelectedSlotsMasks.setUnchecked(trackIndex, selectedSlotsMask.toString(2));
            const ValueTree &firstProcessor = track.getChild(0);
            int slot = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
            newFocusedSlot = {trackIndex, slot};
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

private:
    JUCE_DECLARE_NON_COPYABLE(SelectTrackAction)
};
