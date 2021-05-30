#include "MoveSelectedItems.h"

#include "InsertProcessor.h"

static int limitTrackDelta(int originalTrackDelta, bool anyTrackSelected, bool multipleTracksWithSelections, Tracks &tracks) {
    // If more than one track has any selected items, or if any track itself is selected,
    // don't move any the processors from a non-master track to the master track, or move
    // a full track into the master track slot.
    int maxAllowedTrackIndex = anyTrackSelected || multipleTracksWithSelections ? tracks.getNumNonMasterTracks() - 1 : tracks.size() - 1;

    const auto &firstTrackWithSelections = tracks.findFirstTrackWithSelections();
    const auto &lastTrackWithSelections = tracks.findLastTrackWithSelections();

    return std::clamp(originalTrackDelta, -tracks.indexOfTrack(firstTrackWithSelections),
                      maxAllowedTrackIndex - tracks.indexOfTrack(lastTrackWithSelections));
}

static ValueTree lastNonSelectedProcessorWithSlotLessThan(const Track *track, int slot) {
    const auto &lane = track->getProcessorLane();
    for (int i = lane.getNumChildren() - 1; i >= 0; i--) {
        const auto &processor = lane.getChild(i);
        if (Processor::getSlot(processor) < slot &&
            !track->isProcessorSelected(processor))
            return processor;
    }
    return {};
}

static Array<ValueTree> getFirstProcessorInEachContiguousSelectedGroup(const Track *track) {
    Array<ValueTree> firstProcessorInEachContiguousSelectedGroup;
    int lastSelectedProcessorSlot = -2;
    for (const auto &processor : track->getProcessorLane()) {
        int slot = Processor::getSlot(processor);
        if (slot > lastSelectedProcessorSlot + 1 && track->isSlotSelected(slot)) {
            lastSelectedProcessorSlot = slot;
            firstProcessorInEachContiguousSelectedGroup.add(processor);
        }
    }
    return firstProcessorInEachContiguousSelectedGroup;
}

static int limitSlotDelta(int originalSlotDelta, int limitedTrackDelta, Tracks &tracks, View &view) {
    int limitedSlotDelta = originalSlotDelta;
    for (const auto *fromTrack : tracks.getChildren()) {
        if (fromTrack->isSelected()) continue; // entire track will be moved, so it shouldn't restrict other slot movements

        const auto &lastSelectedProcessor = fromTrack->findLastSelectedProcessor();
        if (!lastSelectedProcessor.isValid()) continue; // no processors to move

        const auto *toTrack = tracks.getChild(fromTrack->getIndex() + limitedTrackDelta);
        const auto &firstSelectedProcessor = fromTrack->findFirstSelectedProcessor(); // valid since lastSelected is valid
        int maxAllowedSlot = view.getNumProcessorSlots(toTrack->isMaster()) - 1;
        limitedSlotDelta = std::clamp(limitedSlotDelta, -Processor::getSlot(firstSelectedProcessor), maxAllowedSlot - Processor::getSlot(lastSelectedProcessor));

        // ---------- Expand processor movement while limiting dynamic processor row creation ---------- //

        // If this move would add new processor rows, make sure we're doing it for good reason!
        // Only force new rows to be added if the selected group is being explicitly dragged to underneath
        // at least one processor.
        //
        // Find the largest slot-delta, less than the original given slot-delta, such that a contiguous selected
        // group in the pre-move track would end up completely below a non-selected processor in
        // the post-move track.
        for (const auto &processor : getFirstProcessorInEachContiguousSelectedGroup(fromTrack)) {
            const auto &lastNonSelected = lastNonSelectedProcessorWithSlotLessThan(toTrack, Processor::getSlot(processor) + originalSlotDelta);
            if (lastNonSelected.isValid()) {
                int candidateSlotDelta = Processor::getSlot(lastNonSelected) + 1 - Processor::getSlot(processor);
                if (candidateSlotDelta <= originalSlotDelta)
                    limitedSlotDelta = std::max(limitedSlotDelta, candidateSlotDelta);
            }
        }
    }

    return limitedSlotDelta;
}

// This is done in three phases.
// * _Handle edge cases_, such as when both master-track and non-master-tracks have selections.
// * _Limit_ the track/slot-delta to the obvious left/right/top/bottom boundaries
// * _Expand_ the slot-delta just enough to allow groups of selected processors to move below non-selected processors,
//   while only creating new processor rows if necessary.
static juce::Point<int> limitedDelta(juce::Point<int> fromTrackAndSlot, juce::Point<int> toTrackAndSlot, Tracks &tracks, View &view) {
    auto originalDelta = toTrackAndSlot - fromTrackAndSlot;
    bool multipleTracksWithSelections = tracks.moreThanOneTrackHasSelections();
    // In the special case that multiple tracks have selections and the master track is one of them,
    // disallow movement because it doesn't make sense dragging horizontally and vertically at the same time.
    const auto *masterTrack = tracks.getMasterTrack();
    if (multipleTracksWithSelections && masterTrack != nullptr && masterTrack->hasSelections())
        return {0, 0};

    bool anyTrackSelected = tracks.anyTrackSelected();

    // When dragging from a non-master track to the master track, interpret as dragging beyond the y-limit,
    // to whatever track slot corresponding to the master track x-position (x/y is flipped in master track).
    if (multipleTracksWithSelections &&
        !Track::isMaster(tracks.getTrackState(fromTrackAndSlot.x)) &&
        Track::isMaster(tracks.getTrackState(toTrackAndSlot.x))) {
        originalDelta = {toTrackAndSlot.y - fromTrackAndSlot.x, view.getNumProcessorSlots() - 1 - fromTrackAndSlot.y};
    }

    int limitedTrackDelta = limitTrackDelta(originalDelta.x, anyTrackSelected, multipleTracksWithSelections, tracks);
    if (fromTrackAndSlot.y == -1) // track-move only
        return {limitedTrackDelta, 0};

    int limitedSlotDelta = limitSlotDelta(originalDelta.y, limitedTrackDelta, tracks, view);
    return {limitedTrackDelta, limitedSlotDelta};
}

MoveSelectedItems::MoveSelectedItems(juce::Point<int> fromTrackAndSlot, juce::Point<int> toTrackAndSlot, bool makeInvalidDefaultsIntoCustom, Tracks &tracks, Connections &connections,
                                     View &view, Input &input, Output &output, ProcessorGraph &processorGraph)
        : trackAndSlotDelta(limitedDelta(fromTrackAndSlot, toTrackAndSlot, tracks, view)),
          updateSelectionAction(trackAndSlotDelta, tracks, connections, view, input, processorGraph),
          insertTrackOrProcessorActions(createInserts(tracks, view)),
          updateConnectionsAction(makeInvalidDefaultsIntoCustom, true, tracks, connections, input, output,
                                  processorGraph, updateSelectionAction.getNewFocusedTrack()) {
    // cleanup - yeah it's ugly but avoids need for some copy/move madness in createUpdateConnectionsAction
    for (int i = insertTrackOrProcessorActions.size() - 1; i >= 0; i--)
        insertTrackOrProcessorActions.getUnchecked(i)->undo();
}

bool MoveSelectedItems::perform() {
    if (insertTrackOrProcessorActions.isEmpty()) return false;

    for (auto *insertAction : insertTrackOrProcessorActions)
        insertAction->perform();
    updateSelectionAction.perform();
    updateConnectionsAction.perform();
    return true;
}

bool MoveSelectedItems::undo() {
    if (insertTrackOrProcessorActions.isEmpty()) return false;

    for (int i = insertTrackOrProcessorActions.size() - 1; i >= 0; i--)
        insertTrackOrProcessorActions.getUnchecked(i)->undo();
    updateSelectionAction.undo();
    updateConnectionsAction.undo();
    return true;
}

OwnedArray<UndoableAction> MoveSelectedItems::createInserts(Tracks &tracks, View &view) const {
    OwnedArray<UndoableAction> insertActions;
    if (trackAndSlotDelta.x == 0 && trackAndSlotDelta.y == 0)
        return insertActions;

    auto addInsertsForTrackIndex = [&](int fromTrackIndex) {
        const auto *fromTrack = tracks.getChild(fromTrackIndex);
        const int toTrackIndex = fromTrackIndex + trackAndSlotDelta.x;

        if (fromTrack == nullptr) return;
        if (fromTrack->isSelected()) {
            if (fromTrackIndex != toTrackIndex) {
                insertActions.add(new InsertTrackAction(fromTrackIndex, toTrackIndex, tracks));
                insertActions.getLast()->perform();
            }
            return;
        }

        if (!fromTrack->findFirstSelectedProcessor().isValid()) return;

        const auto selectedProcessors = fromTrack->findSelectedProcessors();
        auto addInsertsForProcessor = [&](const ValueTree &processor) {
            auto toSlot = Processor::getSlot(processor) + trackAndSlotDelta.y;
            insertActions.add(new InsertProcessor(processor, toTrackIndex, toSlot, tracks, view));
            // Need to actually _do_ the move for each processor, since this could affect the results of
            // a later track's slot moves. i.e. if trackAndSlotDelta.x == -1, then we need to move selected processors
            // out of this track before advancing to the next track. (This action is undone later.)
            insertActions.getLast()->perform();
        };

        if (trackAndSlotDelta.x == 0 && trackAndSlotDelta.y > 0) {
            for (int processorIndex = selectedProcessors.size() - 1; processorIndex >= 0; processorIndex--)
                addInsertsForProcessor(selectedProcessors.getUnchecked(processorIndex));
        } else {
            for (const auto &processor : selectedProcessors)
                addInsertsForProcessor(processor);
        }
    };

    if (trackAndSlotDelta.x <= 0) {
        for (int trackIndex = 0; trackIndex < tracks.size(); trackIndex++)
            addInsertsForTrackIndex(trackIndex);
    } else {
        for (int trackIndex = tracks.size() - 1; trackIndex >= 0; trackIndex--)
            addInsertsForTrackIndex(trackIndex);
    }

    return insertActions;
}

MoveSelectedItems::MoveSelectionsAction::MoveSelectionsAction(juce::Point<int> trackAndSlotDelta, Tracks &tracks, Connections &connections, View &view, Input &input,
                                                              ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, processorGraph) {
    if (trackAndSlotDelta.y != 0) {
        for (int i = 0; i < tracks.size(); i++) {
            if (oldTrackSelections.getUnchecked(i))
                continue; // track itself is being moved, so don't move its selected slots
            const auto &track = tracks.getTrackState(i);
            const auto &lane = Track::getProcessorLane(track);
            auto selectedSlotsMask = ProcessorLane::getSelectedSlotsMask(lane);
            selectedSlotsMask.shiftBits(trackAndSlotDelta.y, 0);
            newSelectedSlotsMasks.setUnchecked(i, selectedSlotsMask);
        }
    }
    if (trackAndSlotDelta.x != 0) {
        auto moveTrackSelections = [&](int fromTrackIndex) {
            int toTrackIndex = fromTrackIndex + trackAndSlotDelta.x;
            if (toTrackIndex >= 0 && toTrackIndex < newSelectedSlotsMasks.size()) {
                newTrackSelections.setUnchecked(toTrackIndex, newTrackSelections.getUnchecked(fromTrackIndex));
                newTrackSelections.setUnchecked(fromTrackIndex, false);
                newSelectedSlotsMasks.setUnchecked(toTrackIndex, newSelectedSlotsMasks.getUnchecked(fromTrackIndex));
                newSelectedSlotsMasks.setUnchecked(fromTrackIndex, BigInteger());
            }
        };
        if (trackAndSlotDelta.x < 0) {
            for (int fromTrackIndex = 0; fromTrackIndex < tracks.size(); fromTrackIndex++) {
                moveTrackSelections(fromTrackIndex);
            }
        } else if (trackAndSlotDelta.x > 0) {
            for (int fromTrackIndex = tracks.size() - 1; fromTrackIndex >= 0; fromTrackIndex--) {
                moveTrackSelections(fromTrackIndex);
            }
        }
    }

    setNewFocusedSlot(oldFocusedSlot + trackAndSlotDelta, false);
}

MoveSelectedItems::InsertTrackAction::InsertTrackAction(int fromTrackIndex, int toTrackIndex, Tracks &tracks)
        : fromTrackIndex(fromTrackIndex), toTrackIndex(toTrackIndex), tracks(tracks) {
}

bool MoveSelectedItems::InsertTrackAction::perform() {
    tracks.getState().moveChild(fromTrackIndex, toTrackIndex, nullptr);
    return true;
}

bool MoveSelectedItems::InsertTrackAction::undo() {
    tracks.getState().moveChild(toTrackIndex, fromTrackIndex, nullptr);
    return true;
}
