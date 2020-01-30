#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"

struct SelectProcessorSlotAction : public SelectAction {
    SelectProcessorSlotAction(const ValueTree& track, int slot, bool selected, bool deselectOthers,
                              TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager,
                              InputStateManager &inputManager, StatefulAudioProcessorContainer &audioProcessorContainer)
            : SelectAction(tracksManager, connectionsManager, viewManager, inputManager, audioProcessorContainer) {
        jassert(track.isValid());

        const auto currentSlotMask = tracksManager.getSlotMask(track);
        if (deselectOthers) {
            for (int i = 0; i < newTrackSelections.size(); i++) {
                newTrackSelections.setUnchecked(i, false);
                newSelectedSlotsMasks.setUnchecked(i, BigInteger().toString(2));
            }
        }

        auto newSlotMask = deselectOthers ? BigInteger() : currentSlotMask;
        newSlotMask.setBit(slot, selected);
        auto trackIndex = tracksManager.indexOf(track);
        newSelectedSlotsMasks.setUnchecked(trackIndex, newSlotMask.toString(2));
        if (selected) {
            setNewFocusedSlot({trackIndex, slot});
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE(SelectProcessorSlotAction)
};
