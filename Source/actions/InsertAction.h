#pragma once

#include <state/OutputState.h>
#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct InsertAction : UndoableAction {
    InsertAction(const ValueTree &copiedState, const juce::Point<int> toTrackAndSlot,
                 TracksState &tracks, ViewState &view, StatefulAudioProcessorContainer &audioProcessorContainer)
            : copiedState(copiedState.createCopy()), tracks(tracks), view(view), audioProcessorContainer(audioProcessorContainer),
              fromTrackAndSlot(findFromTrackAndSlot()), toTrackAndSlot(limitToTrackAndSlot(toTrackAndSlot)) {
        auto trackAndSlotDiff = this->toTrackAndSlot - this->fromTrackAndSlot;

        if (tracks.getMasterTrack().isValid() && toTrackAndSlot.x == tracks.getNumNonMasterTracks()) {
            // When inserting into master track, only insert the processors of the first track with selections
            copyProcessorsFromTrack(copiedState.getChild(fromTrackAndSlot.x), tracks.getNumNonMasterTracks(), trackAndSlotDiff.y);
        } else {
            // First pass: insert processors that are selected without their parent track also selected.
            // This is done because adding new tracks changes the track indices relative to their current position.
            for (const auto &copiedTrack : copiedState) {
                if (!copiedTrack[IDs::selected]) {
                    int toTrackIndex = copiedState.indexOf(copiedTrack) + trackAndSlotDiff.x;
                    if (copiedTrack.getNumChildren() > 0) {
                        while (toTrackIndex >= tracks.getNumNonMasterTracks()) {
                            addAndPerformAction(new CreateTrackAction(false, false, {}, tracks, view));
                        }
                    }
                    if (toTrackIndex < tracks.getNumNonMasterTracks())
                        copyProcessorsFromTrack(copiedTrack, toTrackIndex, trackAndSlotDiff.y);
                }
            }
            // Second pass: insert selected tracks (along with their processors)
            for (const auto &track : copiedState) {
                if (track[IDs::selected]) {
                    int toTrackIndex = copiedState.indexOf(track) + trackAndSlotDiff.x + 1;
                    addAndPerformAction(new CreateTrackAction(toTrackIndex, false, false,
                                                              track, tracks, view));
                    for (const auto &processor : track) {
                        addAndPerformAction(new CreateProcessorAction(processor.createCopy(), toTrackIndex, processor[IDs::processorSlot],
                                                                      tracks, view, audioProcessorContainer));
                    }
                }
            }
        }

        // Cleanup
        for (int i = createActions.size() - 1; i >= 0; i--) {
            auto *action = createActions.getUnchecked(i);
            if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(action))
                createProcessorAction->undoTemporary();
            else
                action->undo();
        }
    }

    bool perform() override {
        if (createActions.isEmpty())
            return false;

        for (auto *createAction : createActions)
            createAction->perform();
        return true;
    }

    bool undo() override {
        if (createActions.isEmpty())
            return false;

        for (int i = createActions.size() - 1; i >= 0; i--)
            createActions.getUnchecked(i)->undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:

    ValueTree copiedState;

    TracksState &tracks;
    ViewState &view;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    juce::Point<int> fromTrackAndSlot;
    juce::Point<int> toTrackAndSlot;

    OwnedArray<UndoableAction> createActions;

    juce::Point<int> findFromTrackAndSlot() {
        int fromTrackIndex = getIndexOfFirstCopiedTrackWithSelections();
        if (anyCopiedTrackSelected())
            return {fromTrackIndex, 0};

        int fromSlot = INT_MAX;
        for (const auto &track : copiedState) {
            int lowestSelectedSlotForTrack = tracks.getSlotMask(track).findNextSetBit(0);
            if (lowestSelectedSlotForTrack != -1)
                fromSlot = std::min(fromSlot, lowestSelectedSlotForTrack);
        }

        assert(fromSlot != INT_MAX);
        return {fromTrackIndex, fromSlot};
    }

    juce::Point<int> limitToTrackAndSlot(juce::Point<int> toTrackAndSlot) {
        return {toTrackAndSlot.x, anyCopiedTrackSelected() ? 0 : toTrackAndSlot.y};
    }

    bool anyCopiedTrackSelected() {
        for (const ValueTree &track : copiedState)
            if (track[IDs::selected])
                return true;

        return false;
    }

    int getIndexOfFirstCopiedTrackWithSelections() {
        for (const auto &track : copiedState)
            if (track[IDs::selected] || tracks.trackHasAnySlotSelected(track))
                return copiedState.indexOf(track);

        assert(false); // Copied state, by definition, must have a selection.
    }

    void copyProcessorsFromTrack(const ValueTree &fromTrack, int toTrackIndex, int slotDiff) {
        const BigInteger slotsMask = TracksState::getSlotMask(fromTrack);
        for (int slot = 0; slot <= slotsMask.getHighestBit(); slot++) {
            if (slotsMask[slot]) {
                addAndPerformAction(new CreateProcessorAction(TracksState::getProcessorAtSlot(fromTrack, slot).createCopy(),
                                                              toTrackIndex, slot + slotDiff,
                                                              tracks, view, audioProcessorContainer));
            }
        }
    }

    void addAndPerformAction(UndoableAction *action) {
        if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(action))
            createProcessorAction->performTemporary();
        else
            action->perform();
        createActions.add(action);
    }

    JUCE_DECLARE_NON_COPYABLE(InsertAction)
};
