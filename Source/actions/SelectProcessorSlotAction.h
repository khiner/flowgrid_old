#pragma once

#include "SelectAction.h"

struct SelectProcessorSlotAction : public SelectAction {
    SelectProcessorSlotAction(const ValueTree &track, int slot, bool selected, bool deselectOthers,
                              TracksState &tracks, ConnectionsState &connections, ViewState &view,
                              InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
            : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
        const auto currentSlotMask = TracksState::getSlotMask(track);
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

private:
    JUCE_DECLARE_NON_COPYABLE(SelectProcessorSlotAction)
};
