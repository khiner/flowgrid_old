#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

struct MoveSelectedItemsAction : UndoableAction {
    MoveSelectedItemsAction(juce::Point<int> fromTrackAndSlot, juce::Point<int> toTrackAndSlot, bool makeInvalidDefaultsIntoCustom,
                            TracksState &tracks, ConnectionsState &connections, ViewState &view,
                            InputState &input, OutputState &output, StatefulAudioProcessorContainer &audioProcessorContainer)
            : trackAndSlotDelta(limitedDelta(fromTrackAndSlot, toTrackAndSlot, tracks, view)),
              updateSelectionAction(trackAndSlotDelta, tracks, connections, view, input, audioProcessorContainer),
              insertTrackOrProcessorActions(createInsertActions(tracks, view)),
              updateConnectionsAction(makeInvalidDefaultsIntoCustom, true, tracks, connections, input, output,
                                      audioProcessorContainer, updateSelectionAction.getNewFocusedTrack()) {
        // cleanup - yeah it's ugly but avoids need for some copy/move madness in createUpdateConnectionsAction
        for (int i = insertTrackOrProcessorActions.size() - 1; i >= 0; i--)
            insertTrackOrProcessorActions.getUnchecked(i)->undo();
    }

    bool perform() override {
        if (insertTrackOrProcessorActions.isEmpty())
            return false;

        for (auto* insertAction : insertTrackOrProcessorActions)
            insertAction->perform();
        updateSelectionAction.perform();
        updateConnectionsAction.perform();
        return true;
    }

    bool undo() override {
        if (insertTrackOrProcessorActions.isEmpty())
            return false;

        for (int i = insertTrackOrProcessorActions.size() - 1; i >= 0; i--)
            insertTrackOrProcessorActions.getUnchecked(i)->undo();
        updateSelectionAction.undo();
        updateConnectionsAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:

    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(juce::Point<int> trackAndSlotDelta,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
                : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
            if (trackAndSlotDelta.y != 0) {
                for (int i = 0; i < tracks.getNumTracks(); i++) {
                    if (oldTrackSelections.getUnchecked(i))
                        continue; // track itself is being moved, so don't move its selected slots
                    const auto& track = tracks.getTrack(i);
                    BigInteger selectedSlotsMask;
                    selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
                    selectedSlotsMask.shiftBits(trackAndSlotDelta.y, 0);
                    newSelectedSlotsMasks.setUnchecked(i, selectedSlotsMask.toString(2));
                }
            }
            if (trackAndSlotDelta.x != 0) {
                auto moveTrackSelections = [&](int fromTrackIndex) {
                    int toTrackIndex = fromTrackIndex + trackAndSlotDelta.x;
                    if (toTrackIndex >= 0 && toTrackIndex < newSelectedSlotsMasks.size()) {
                        const auto &toTrack = tracks.getTrack(toTrackIndex);
                        newTrackSelections.setUnchecked(toTrackIndex, newTrackSelections.getUnchecked(fromTrackIndex));
                        newTrackSelections.setUnchecked(fromTrackIndex, false);
                        newSelectedSlotsMasks.setUnchecked(toTrackIndex, newSelectedSlotsMasks.getUnchecked(fromTrackIndex));
                        newSelectedSlotsMasks.setUnchecked(fromTrackIndex, BigInteger().toString(2));
                    }
                };
                if (trackAndSlotDelta.x < 0) {
                    for (int fromTrackIndex = 0; fromTrackIndex < tracks.getNumTracks(); fromTrackIndex++) {
                        moveTrackSelections(fromTrackIndex);
                    }
                } else if (trackAndSlotDelta.x > 0) {
                    for (int fromTrackIndex = tracks.getNumTracks() - 1; fromTrackIndex >= 0; fromTrackIndex--) {
                        moveTrackSelections(fromTrackIndex);
                    }
                }
            }

            setNewFocusedSlot(oldFocusedSlot + trackAndSlotDelta, false);
        }
    };

    struct InsertTrackAction : UndoableAction {
        InsertTrackAction(int fromTrackIndex, int toTrackIndex, TracksState &tracks)
                : fromTrackIndex(fromTrackIndex), toTrackIndex(toTrackIndex), tracks(tracks) {
        }

        bool perform() override {
            tracks.getState().moveChild(fromTrackIndex, toTrackIndex, nullptr);
            return true;
        }

        bool undo() override {
            tracks.getState().moveChild(toTrackIndex, fromTrackIndex, nullptr);
            return true;
        }

        int fromTrackIndex, toTrackIndex;

        TracksState &tracks;
    };

    juce::Point<int> trackAndSlotDelta;
    MoveSelectionsAction updateSelectionAction;
    OwnedArray<UndoableAction> insertTrackOrProcessorActions;
    UpdateAllDefaultConnectionsAction updateConnectionsAction;

    // As a side effect, this method actually performs the processor/track moves in preparation for
    // `createUpdateConnectionsAction`, which should be called immediately after this.
    // This avoids a redundant `undo` on all insert actions here, as well as the subsequent
    // `perform` that would be needed in `createUpdateConnectionsAction` to find the new default connections.
    OwnedArray<UndoableAction> createInsertActions(TracksState &tracks, ViewState &view) {
        OwnedArray<UndoableAction> insertActions;

        if (trackAndSlotDelta.x == 0 && trackAndSlotDelta.y == 0)
            return insertActions;

        auto addInsertActionsForTrackIndex = [&](int fromTrackIndex) {
            const auto& fromTrack = tracks.getTrack(fromTrackIndex);
            const int toTrackIndex = fromTrackIndex + trackAndSlotDelta.x;

            if (fromTrack[IDs::selected]) {
                if (fromTrackIndex != toTrackIndex) {
                    insertActions.add(new InsertTrackAction(fromTrackIndex, toTrackIndex, tracks));
                    insertActions.getLast()->perform();
                }
                return;
            }

            if (!TracksState::findFirstSelectedProcessor(fromTrack).isValid())
                return;

            const auto selectedProcessors = TracksState::findSelectedProcessorsForTrack(fromTrack);

            auto addInsertActionsForProcessor = [&](const ValueTree& processor) {
                auto toSlot = int(processor[IDs::processorSlot]) + trackAndSlotDelta.y;
                insertActions.add(new InsertProcessorAction(processor, toTrackIndex, toSlot, tracks, view));
                // Need to actually _do_ the move for each processor, since this could affect the results of
                // a later track's slot moves. i.e. if trackAndSlotDelta.x == -1, then we need to move selected processors
                // out of this track before advancing to the next track. (This action is undone later.)
                insertActions.getLast()->perform();
            };

            if (trackAndSlotDelta.x == 0 && trackAndSlotDelta.y > 0) {
                for (int processorIndex = selectedProcessors.size() - 1; processorIndex >= 0; processorIndex--)
                    addInsertActionsForProcessor(selectedProcessors.getUnchecked(processorIndex));
            } else {
                for (const auto& processor : selectedProcessors)
                    addInsertActionsForProcessor(processor);
            }
        };

        if (trackAndSlotDelta.x <= 0) {
            for (int trackIndex = 0; trackIndex < tracks.getNumTracks(); trackIndex++)
                addInsertActionsForTrackIndex(trackIndex);
        } else {
            for (int trackIndex = tracks.getNumTracks() - 1; trackIndex >= 0; trackIndex--)
                addInsertActionsForTrackIndex(trackIndex);
        }

        return insertActions;
    }

    // This is done in three phases.
    // * _Handle edge cases_, such as when both master-track and non-master-tracks have selections.
    // * _Limit_ the track/slot-delta to the obvious left/right/top/bottom boundaries, with appropriate special cases for mixer channel slots.
    // * _Expand_ the slot-delta just enough to allow groups of selected processors to move below non-selected processors,
    //   while only creating new processor rows if necessary.
    static juce::Point<int> limitedDelta(juce::Point<int> fromTrackAndSlot, juce::Point<int> toTrackAndSlot,
                                         TracksState &tracks, ViewState& view) {
        auto originalDelta = toTrackAndSlot - fromTrackAndSlot;
        bool multipleTracksWithSelections = tracks.moreThanOneTrackHasSelections();
        // In the special case that multiple tracks have selections and the master track is one of them,
        // disallow movement because it doesn't make sense dragging horizontally and vertically at the same time.
        if (multipleTracksWithSelections && TracksState::doesTrackHaveSelections(tracks.getMasterTrack()))
            return {0, 0};

        bool anyTrackSelected = tracks.anyTrackSelected();

        // When dragging from a non-master track to the master track, interpret as dragging beyond the y-limit,
        // to whatever track slot corresponding to the master track x-position (x/y is flipped in master track).
        if (multipleTracksWithSelections &&
            !TracksState::isMasterTrack(tracks.getTrack(fromTrackAndSlot.x)) &&
            TracksState::isMasterTrack(tracks.getTrack(toTrackAndSlot.x))) {
            originalDelta = {toTrackAndSlot.y - fromTrackAndSlot.x, view.getNumTrackProcessorSlots() - 1 - fromTrackAndSlot.y};
        }

        int limitedTrackDelta = limitTrackDelta(originalDelta.x, anyTrackSelected, multipleTracksWithSelections, tracks);
        if (fromTrackAndSlot.y == -1) // track-move only
            return {limitedTrackDelta, 0};

        int limitedSlotDelta = limitSlotDelta(originalDelta.y, limitedTrackDelta, tracks);
        return {limitedTrackDelta, limitedSlotDelta};
    }

    static int limitTrackDelta(int originalTrackDelta, bool anyTrackSelected, bool multipleTracksWithSelections, TracksState &tracks) {
        // If more than one track has any selected items, or if any track itself is selected,
        // don't move any the processors from a non-master track to the master track, or move
        // a full track into the master track slot.
        int maxAllowedTrackIndex = anyTrackSelected || multipleTracksWithSelections ? tracks.getNumNonMasterTracks() - 1 : tracks.getNumTracks() - 1;

        const auto& firstTrackWithSelectedProcessors = tracks.findFirstTrackWithSelectedProcessors();
        const auto& lastTrackWithSelectedProcessors = tracks.findLastTrackWithSelectedProcessors();
        return std::clamp(originalTrackDelta, -tracks.indexOf(firstTrackWithSelectedProcessors),
                          maxAllowedTrackIndex - tracks.indexOf(lastTrackWithSelectedProcessors));
    }

    static int limitSlotDelta(int originalSlotDelta, int limitedTrackDelta, TracksState &tracks) {
        int limitedSlotDelta = originalSlotDelta;
        for (const auto& fromTrack : tracks.getState()) {
            if (fromTrack[IDs::selected])
                continue; // entire track will be moved, so it shouldn't restrict other slot movements

            const auto& lastSelectedProcessor = TracksState::findLastSelectedProcessor(fromTrack);
            if (!lastSelectedProcessor.isValid())
                continue; // no processors to move

            // Mixer channels can be dragged into the reserved last slot of each track if it doesn't already hold a mixer channel.
            const auto& toTrack = tracks.getTrack(tracks.indexOf(fromTrack) + limitedTrackDelta);
            int maxAllowedSlot = tracks.getMixerChannelSlotForTrack(toTrack) - 1;
            const ValueTree &mixerChannel = tracks.getMixerChannelProcessorForTrack(toTrack);
            if ((!mixerChannel.isValid() || lastSelectedProcessor == mixerChannel) &&
                TracksState::isMixerChannelProcessor(lastSelectedProcessor))
                maxAllowedSlot += 1;

            const auto& firstSelectedProcessor = TracksState::findFirstSelectedProcessor(fromTrack); // valid since lastSelected is valid
            const int firstSelectedSlot = firstSelectedProcessor[IDs::processorSlot];
            const int lastSelectedSlot = lastSelectedProcessor[IDs::processorSlot];
            limitedSlotDelta = std::clamp(limitedSlotDelta, -firstSelectedSlot, maxAllowedSlot - lastSelectedSlot);

            // ---------- Expand processor movement while limiting dynamic processor row creation ---------- //

            // If this move would add new processor rows, make sure we're doing it for good reason!
            // Only force new rows to be added if the selected group is being explicitly dragged to underneath
            // at least one non-mixer processor.
            //
            // Find the largest slot-delta, less than the original given slot-delta, such that a contiguous selected
            // group in the pre-move track would end up _completely below a non-selected, non-mixer_ processor in
            // the post-move track.
            for (const auto& processor : getFirstProcessorInEachContiguousSelectedGroup(fromTrack)) {
                int toSlot = int(processor[IDs::processorSlot]) + originalSlotDelta;
                const auto& lastNonSelected = lastNonSelectedNonMixerProcessorWithSlotLessThan(toTrack, toSlot);
                if (lastNonSelected.isValid()) {
                    int candidateSlotDelta = int(lastNonSelected[IDs::processorSlot]) + 1 - int(processor[IDs::processorSlot]);
                    if (candidateSlotDelta <= originalSlotDelta)
                        limitedSlotDelta = std::max(limitedSlotDelta, candidateSlotDelta);
                }
            }
        }

        return limitedSlotDelta;
    }

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

    static Array<ValueTree> getFirstProcessorInEachContiguousSelectedGroup(const ValueTree& track) {
        Array<ValueTree> firstProcessorInEachContiguousSelectedGroup;
        int lastSelectedProcessorSlot = -2;
        for (const auto& processor : track) {
            int slot = processor[IDs::processorSlot];
            if (slot > lastSelectedProcessorSlot + 1 && TracksState::isSlotSelected(track, slot)) {
                lastSelectedProcessorSlot = slot;
                firstProcessorInEachContiguousSelectedGroup.add(processor);
            }
        }
        return firstProcessorInEachContiguousSelectedGroup;
    }

    JUCE_DECLARE_NON_COPYABLE(MoveSelectedItemsAction)
};
