#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

struct MoveSelectedItemsAction : UndoableAction {
    MoveSelectedItemsAction(juce::Point<int> fromGridPoint, juce::Point<int> toGridPoint, bool makeInvalidDefaultsIntoCustom,
                            TracksState &tracks, ConnectionsState &connections, ViewState &view,
                            InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), connections(connections), view(view), input(input),
              audioProcessorContainer(audioProcessorContainer),
              fromGridPoint(fromGridPoint),
              gridDelta(limitGridDelta(toGridPoint - fromGridPoint)),
              insertActions(createInsertActions()),
              updateSelectionAction(createUpdateSelectionAction()),
              updateConnectionsAction(createUpdateConnectionsAction(makeInvalidDefaultsIntoCustom)) {
        // cleanup - yeah it's ugly but avoids need for some copy/move madness in createUpdateConnectionsAction
        for (int i = insertActions.size() - 1; i >= 0; i--)
            insertActions.getUnchecked(i)->undo();
    }

    bool perform() override {
        if (insertActions.isEmpty())
            return false;

        for (auto* insertAction : insertActions)
            insertAction->perform();
        updateSelectionAction.perform();
        updateConnectionsAction.perform();
        return true;
    }

    bool undo() override {
        if (insertActions.isEmpty())
            return false;

        for (int i = insertActions.size() - 1; i >= 0; i--)
            insertActions.getUnchecked(i)->undo();
        updateSelectionAction.undo();
        updateConnectionsAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:

    static ValueTree lastNonSelectedNonMixerProcessorWithSlotLessThan(const ValueTree& track, int slot) {
        for (int i = track.getNumChildren() - 1; i >= 0; i--) {
            const auto& processor = track.getChild(i);
            if (int(processor[IDs::processorSlot]) < slot &&
                !TracksState::isMixerChannelProcessor(processor) &&
                !TracksState::isProcessorSelected(processor))
                return processor;
        }
        return {};
    }

    // This is done in three phases.
    // * Handle cases when there are both master-track and non-master-track selections.
    // * _Limit_ the x/y delta to the obvious left/right/top/bottom boundaries, with appropriate special cases for mixer channel slots.
    // * _Expand_ the slot-delta just enough to allow groups of selected processors to move below non-selected processors.
    // (The principle here is to only create new processor rows if necessary.)
    juce::Point<int> limitGridDelta(juce::Point<int> gridDelta) {
        // ---------- Special cases ---------- //

        bool multipleTracksSelected = tracks.doesMoreThanOneTrackHaveSelections();
        // In the special case that multiple tracks have selections and the master track is one of them,
        // disallow movement because it doesn't make sense dragging horizontally and vertically at the same time.
        if (multipleTracksSelected && tracks.doesTrackHaveSelections(tracks.getMasterTrack()))
            return {0, 0};

        auto originalGridDelta = gridDelta; // used for y-expand phase after limiting gridDelta

        // When dragging from a non-master track to the master track,
        // interpret as dragging beyond the y-limit, to whatever track slot corresponding to the master track x-grid-position (toSlot.y)
        if (multipleTracksSelected &&
            !TracksState::isMasterTrack(tracks.getTrack(fromGridPoint.x)) &&
            TracksState::isMasterTrack(tracks.getTrack((fromGridPoint + gridDelta).x)))
            gridDelta = {gridDelta.y, view.getNumTrackProcessorSlots() - 2};


        // ---------- Limit track movement ---------- //

        // If more than one track has any selected items, don't move any the processors from a non-master track to the master track
        int maxAllowedTrackIndex = multipleTracksSelected ? tracks.getNumNonMasterTracks() - 1 : tracks.getNumTracks() - 1;

        const auto& firstTrackWithSelectedProcessors = tracks.findFirstTrackWithSelectedProcessors();
        const auto& lastTrackWithSelectedProcessors = tracks.findLastTrackWithSelectedProcessors();
        gridDelta.x = std::clamp(gridDelta.x, -tracks.indexOf(firstTrackWithSelectedProcessors),
                                 maxAllowedTrackIndex - tracks.indexOf(lastTrackWithSelectedProcessors));


        // ---------- Limit processor movement ---------- //

        for (const auto& fromTrack : tracks.getState()) {
            const auto& lastSelectedProcessor = TracksState::findLastSelectedProcessor(fromTrack);
            if (!lastSelectedProcessor.isValid())
                continue; // no selections to move

            // Mixer channels can be dragged into the reserved last slot of each track if it doesn't already hold a mixer channel.
            const auto& toTrack = tracks.getTrack(tracks.indexOf(fromTrack) + gridDelta.x);
            int maxAllowedSlot = tracks.getMixerChannelSlotForTrack(toTrack) - 1;
            if (!tracks.getMixerChannelProcessorForTrack(toTrack).isValid() &&
                TracksState::isMixerChannelProcessor(lastSelectedProcessor))
                maxAllowedSlot += 1;

            const auto& firstSelectedProcessor = TracksState::findFirstSelectedProcessor(fromTrack); // valid since lastSelected is valid
            const int firstSelectedSlot = firstSelectedProcessor[IDs::processorSlot];
            const int lastSelectedSlot = lastSelectedProcessor[IDs::processorSlot];
            gridDelta.y = std::clamp(gridDelta.y, -firstSelectedSlot, maxAllowedSlot - lastSelectedSlot);
        }

        // ---------- Expand processor movement while limiting dynamic processor row creation ---------- //

        // If this move would add new processor rows, make sure we're doing it for good reason!
        // Only force new rows to be added if the selected group is being explicitly dragged to underneath
        // at least one non-mixer processor.
        //
        // Find the largest slot-delta, less than the original given slot-delta, such that a contiguous selected
        // group in the pre-move track would end up _completely below a non-selected, non-mixer_ processor in
        // the post-move track.
        for (const auto& oldTrack : tracks.getState()) {
            if (!TracksState::trackHasAnySlotSelected(oldTrack))
                continue;
            const ValueTree& newTrack = tracks.getTrack(tracks.indexOf(oldTrack) + gridDelta.x);
            bool firstSelectedInGroup = false;
            for (int slot = 0; slot < tracks.getNumAvailableSlotsForTrack(oldTrack); slot++) {
                const auto& processor = TracksState::getProcessorAtSlot(oldTrack, slot);
                if (!firstSelectedInGroup && processor.isValid() && TracksState::isProcessorSelected(processor)) {
                    firstSelectedInGroup = true;

                    int newSlot = int(processor[IDs::processorSlot]) + originalGridDelta.y;
                    const auto& lastNonSelected = lastNonSelectedNonMixerProcessorWithSlotLessThan(newTrack, newSlot);
                    if (lastNonSelected.isValid()) {
                        int newSlotDelta = int(lastNonSelected[IDs::processorSlot]) + 1 - int(processor[IDs::processorSlot]);
                        if (newSlotDelta <= originalGridDelta.y)
                            gridDelta.y = std::max(gridDelta.y, newSlotDelta); // TODO feels like there's a break condition here somewhere...
                    }
                } else {
                    firstSelectedInGroup = false;
                }
            }
        }

        return gridDelta;
    }

    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(juce::Point<int> gridDelta,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
                : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
            if (gridDelta.y != 0) {
                for (int i = 0; i < tracks.getNumTracks(); i++) {
                    const auto& track = tracks.getTrack(i);
                    BigInteger selectedSlotsMask;
                    selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
                    selectedSlotsMask.shiftBits(gridDelta.y, 0);
                    newSelectedSlotsMasks.setUnchecked(i, selectedSlotsMask.toString(2));
                }
            }
            if (gridDelta.x != 0) {
                auto moveTrackSelections = [&](int fromTrackIndex) {
                    const auto &fromTrack = tracks.getTrack(fromTrackIndex);
                    int toTrackIndex = fromTrackIndex + gridDelta.x;
                    if (toTrackIndex >= 0 && toTrackIndex < newSelectedSlotsMasks.size()) {
                        const auto &toTrack = tracks.getTrack(toTrackIndex);
                        newSelectedSlotsMasks.setUnchecked(toTrackIndex, newSelectedSlotsMasks.getUnchecked(fromTrackIndex));
                        newSelectedSlotsMasks.setUnchecked(fromTrackIndex, BigInteger().toString(2));
                    }
                };
                if (gridDelta.x < 0) {
                    for (int fromTrackIndex = 0; fromTrackIndex < tracks.getNumTracks(); fromTrackIndex++) {
                        moveTrackSelections(fromTrackIndex);
                    }
                } else if (gridDelta.x > 0) {
                    for (int fromTrackIndex = tracks.getNumTracks() - 1; fromTrackIndex >= 0; fromTrackIndex--) {
                        moveTrackSelections(fromTrackIndex);
                    }
                }
            }
            setNewFocusedSlot(oldFocusedSlot + gridDelta, false);
        }
    };

    TracksState &tracks;
    ConnectionsState &connections;
    ViewState &view;
    InputState &input;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    juce::Point<int> fromGridPoint, gridDelta;
    OwnedArray<InsertProcessorAction> insertActions;
    SelectAction updateSelectionAction;
    UpdateAllDefaultConnectionsAction updateConnectionsAction;

    // As a side effect, this method actually does the processor/track moves in preparation for
    // `createUpdateConnectionsAction`, which should be called immediately after this.
    // This avoids a redundant `undo` on all insert actions here, as well as the subsequent
    // `perform` that would be needed in `createUpdateConnectionsAction` to find the new default connections.
    OwnedArray<InsertProcessorAction> createInsertActions() {
        OwnedArray<InsertProcessorAction> insertActions;
        if (gridDelta.x == 0 && gridDelta.y == 0)
            return insertActions;

        auto addInsertActionsForTrackIndex = [&](int fromTrackIndex) {
            const int toTrackIndex = fromTrackIndex + gridDelta.x;
            const auto selectedProcessors = TracksState::getSelectedProcessorsForTrack(tracks.getTrack(fromTrackIndex));

            auto addInsertActionsForProcessor = [&](const ValueTree& processor) {
                auto toSlot = int(processor[IDs::processorSlot]) + gridDelta.y;
                insertActions.add(new InsertProcessorAction(processor, toTrackIndex, toSlot, tracks, view));
                // Need to actually _do_ the move for each track, since this could affect the results of
                // a later track's slot moves. i.e. if gridDelta.x == -1, then we need to move selected processors
                // out of this track before advancing to the next track. (This action is undone later.)
                insertActions.getLast()->perform();
            };

            if (fromTrackIndex == toTrackIndex && gridDelta.y > 0) {
                for (int processorIndex = selectedProcessors.size() - 1; processorIndex >= 0; processorIndex--)
                    addInsertActionsForProcessor(selectedProcessors.getUnchecked(processorIndex));
            } else {
                for (const auto& processor : selectedProcessors)
                    addInsertActionsForProcessor(processor);
            }
        };

        if (gridDelta.x <= 0) {
            for (int trackIndex = 0; trackIndex < tracks.getNumTracks(); trackIndex++)
                addInsertActionsForTrackIndex(trackIndex);
        } else {
            for (int trackIndex = tracks.getNumTracks() - 1; trackIndex >= 0; trackIndex--)
                addInsertActionsForTrackIndex(trackIndex);
        }

        return insertActions;
    }

    // Assumes all insertActions have been performed (see comment above `createInsertActions`)
    // After determining what the new default connections will be, it moves everything back to where it was.
    UpdateAllDefaultConnectionsAction createUpdateConnectionsAction(bool makeInvalidDefaultsIntoCustom) {
        return UpdateAllDefaultConnectionsAction(makeInvalidDefaultsIntoCustom, true, connections, tracks, input,
                                                 audioProcessorContainer, updateSelectionAction.getNewFocusedTrack());
    }

    SelectAction createUpdateSelectionAction() {
        return MoveSelectionsAction(gridDelta, tracks, connections, view, input, audioProcessorContainer);
    }

    JUCE_DECLARE_NON_COPYABLE(MoveSelectedItemsAction)
};
