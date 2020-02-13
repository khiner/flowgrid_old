#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"
#include "SelectTrackAction.h"

struct SelectRectangleAction : public SelectAction {
    SelectRectangleAction(const juce::Point<int> fromTrackAndSlot, const juce::Point<int> toTrackAndSlot,
                          TracksState &tracks, ConnectionsState &connections, ViewState &view,
                          InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
            : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
        Rectangle<int> selectionRectangle(tracks.trackAndSlotToGridPosition(fromTrackAndSlot), tracks.trackAndSlotToGridPosition(toTrackAndSlot));
        selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

        for (int trackIndex = 0; trackIndex < tracks.getNumTracks(); trackIndex++) {
            auto track = tracks.getTrack(trackIndex);
            bool trackSelected = selectionRectangle.contains(tracks.trackAndSlotToGridPosition({trackIndex, -1}));
            newTrackSelections.setUnchecked(trackIndex, trackSelected);
            if (trackSelected) {
                newSelectedSlotsMasks.setUnchecked(trackIndex, tracks.createFullSelectionBitmask(track));
            } else {
                BigInteger newSlotsMask;
                for (int otherSlot = 0; otherSlot < tracks.getNumAvailableSlotsForTrack(track); otherSlot++)
                    newSlotsMask.setBit(otherSlot, selectionRectangle.contains(tracks.trackAndSlotToGridPosition({trackIndex, otherSlot})));
                newSelectedSlotsMasks.setUnchecked(trackIndex, newSlotsMask.toString(2));
            }
        }
        int slotToFocus = toTrackAndSlot.y;
        if (slotToFocus == -1) {
            const ValueTree &firstProcessor = tracks.getTrack(toTrackAndSlot.x).getChild(0);
            slotToFocus = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
        }
        setNewFocusedSlot({toTrackAndSlot.x, slotToFocus});
    }

private:
    JUCE_DECLARE_NON_COPYABLE(SelectRectangleAction)
};
