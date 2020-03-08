#pragma once

#include <state/OutputState.h>
#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct InsertAction : UndoableAction {
    InsertAction(const ValueTree &copiedState, const juce::Point<int> toTrackAndSlot,
                 TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : copiedState(copiedState.createCopy()), fromTrackAndSlot(findFromTrackAndSlot()), toTrackAndSlot(toTrackAndSlot),
              tracks(tracks), view(view), audioProcessorContainer(audioProcessorContainer) {
    }

    bool perform() override {
        const auto trackAndSlotDiff = toTrackAndSlot - fromTrackAndSlot;

        // First pass: copy processors that are selected without their parent track also selected.
        // This is done because adding new tracks changes the track indices relative to the past position.
        for (const auto &track : copiedState) {
            if (!track[IDs::selected]) {
                int toTrackIndex = copiedState.indexOf(track) + trackAndSlotDiff.x;
                if (track.getNumChildren() > 0)
                    while (toTrackIndex >= tracks.getNumNonMasterTracks())
                        // TODO assumes since there is no mixer channel that it is "derived". Should have a unique track number
                        CreateTrackAction(false, false, {}, tracks, view).perform();
                for (const auto &processor : track) {
                    int toSlot = int(processor[IDs::processorSlot]) + trackAndSlotDiff.y;
                    CreateProcessorAction(processor.createCopy(), toTrackIndex, toSlot,
                                          tracks, view, audioProcessorContainer).perform();
                }
            }
        }
        // Second pass: paste selected tracks (along with their processors
        for (const auto &track : copiedState) {
            if (track[IDs::selected]) {
                int toTrackIndex = copiedState.indexOf(track) + trackAndSlotDiff.x + 1;
                CreateTrackAction(toTrackIndex, false, false, track, tracks, view).perform();
                for (const auto &processor : track) {
                    int toSlot = processor[IDs::processorSlot];
                    CreateProcessorAction(processor.createCopy(), toTrackIndex, toSlot,
                                          tracks, view, audioProcessorContainer).perform();
                }
            }
        }
        return true;
    }

    bool undo() override {
        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:

    ValueTree copiedState;
    juce::Point<int> fromTrackAndSlot;
    juce::Point<int> toTrackAndSlot;

    TracksState &tracks;
    ViewState &view;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    juce::Point<int> findFromTrackAndSlot() {
        juce::Point<int> fromTrackAndSlot = {INT_MAX, INT_MAX};
        for (int trackIndex = 0; trackIndex < copiedState.getNumChildren(); trackIndex++) {
            const auto &track = copiedState.getChild(trackIndex);
            if (fromTrackAndSlot.x == INT_MAX && (track[IDs::selected] || tracks.trackHasAnySlotSelected(track))) {
                fromTrackAndSlot.x = trackIndex;
            }
            if (!track[IDs::selected]) {
                int lowestSelectedSlotForTrack = tracks.getSlotMask(track).findNextSetBit(0);
                if (lowestSelectedSlotForTrack != -1)
                    fromTrackAndSlot.y = std::min(fromTrackAndSlot.y, lowestSelectedSlotForTrack);
            }
        }
        if (fromTrackAndSlot.y == INT_MAX)
            fromTrackAndSlot.y = 0;
        assert(fromTrackAndSlot.x != INT_MAX); // Copied state must, by definition, have a selection.
        return fromTrackAndSlot;
    }

    JUCE_DECLARE_NON_COPYABLE(InsertAction)
};
