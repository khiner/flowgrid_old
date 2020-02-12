#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"
#include "SelectTrackAction.h"

struct SelectRectangleAction : public SelectAction {
    SelectRectangleAction(const juce::Point<int> fromGridPoint, const juce::Point<int> toGridPoint,
                      TracksState &tracks, ConnectionsState &connections, ViewState &view,
                      InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
            : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
        Rectangle<int> selectionRectangle(fromGridPoint, toGridPoint);
        selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

        for (int otherTrackIndex = 0; otherTrackIndex < tracks.getNumTracks(); otherTrackIndex++) {
            auto otherTrack = tracks.getTrack(otherTrackIndex);
            bool trackSelected = (fromGridPoint.y == -1 || toGridPoint.y == -1) &&
                                 ((fromGridPoint.x <= otherTrackIndex && otherTrackIndex <= toGridPoint.x) ||
                                  (toGridPoint.x <= otherTrackIndex && otherTrackIndex <= fromGridPoint.x));
            newTrackSelections.setUnchecked(otherTrackIndex, trackSelected);
            if (trackSelected) {
                newSelectedSlotsMasks.setUnchecked(otherTrackIndex, tracks.createFullSelectionBitmask(otherTrack));
            } else {
                BigInteger newSlotsMask;
                for (int otherSlot = 0; otherSlot < view.getNumAvailableSlotsForTrack(otherTrack); otherSlot++)
                    newSlotsMask.setBit(otherSlot, selectionRectangle.contains(juce::Point(otherTrackIndex, otherSlot)));
                newSelectedSlotsMasks.setUnchecked(otherTrackIndex, newSlotsMask.toString(2));
            }
        }
        int slotToFocus = toGridPoint.y;
        if (slotToFocus == -1) {
            const ValueTree &firstProcessor = tracks.getTrack(toGridPoint.x).getChild(0);
            slotToFocus = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
        }
        setNewFocusedSlot({toGridPoint.x, slotToFocus});
    }

private:
    JUCE_DECLARE_NON_COPYABLE(SelectRectangleAction)
};
