#include "MoveSelectedItems.h"

#include "InsertProcessor.h"

static int limitTrackDelta(int originalTrackDelta, bool anyTrackSelected, bool multipleTracksWithSelections, Tracks &tracks) {
    // If more than one track has any selected items, or if any track itself is selected,
    // don't move any the processors from a non-master track to the master track, or move
    // a full track into the master track slot.
    int maxAllowedTrackIndex = anyTrackSelected || multipleTracksWithSelections ? tracks.getNumNonMasterTracks() - 1 : tracks.getNumTracks() - 1;

    const auto &firstTrackWithSelections = tracks.findFirstTrackWithSelections();
    const auto &lastTrackWithSelections = tracks.findLastTrackWithSelections();

    return std::clamp(originalTrackDelta, -tracks.indexOf(firstTrackWithSelections),
                      maxAllowedTrackIndex - tracks.indexOf(lastTrackWithSelections));
}

static ValueTree lastNonSelectedProcessorWithSlotLessThan(const ValueTree &track, int slot) {
    const auto &lane = Track::getProcessorLane(track);
    for (int i = lane.getNumChildren() - 1; i >= 0; i--) {
        const auto &processor = lane.getChild(i);
        if (Processor::getSlot(processor) < slot &&
            !Track::isProcessorSelected(processor))
            return processor;
    }
    return {};
}

static Array<ValueTree> getFirstProcessorInEachContiguousSelectedGroup(const ValueTree &track) {
    Array<ValueTree> firstProcessorInEachContiguousSelectedGroup;
    int lastSelectedProcessorSlot = -2;
    for (const auto &processor : Track::getProcessorLane(track)) {
        int slot = Processor::getSlot(processor);
        if (slot > lastSelectedProcessorSlot + 1 && Track::isSlotSelected(track, slot)) {
            lastSelectedProcessorSlot = slot;
            firstProcessorInEachContiguousSelectedGroup.add(processor);
        }
    }
    return firstProcessorInEachContiguousSelectedGroup;
}

static int limitSlotDelta(int originalSlotDelta, int limitedTrackDelta, Tracks &tracks, View &view) {
    int limitedSlotDelta = originalSlotDelta;
    for (const auto &fromTrack : tracks.getState()) {
        if (Track::isSelected(fromTrack))
            continue; // entire track will be moved, so it shouldn't restrict other slot movements

        const auto &lastSelectedProcessor = Track::findLastSelectedProcessor(fromTrack);
        if (!lastSelectedProcessor.isValid())
            continue; // no processors to move

        const auto &toTrack = tracks.getTrack(tracks.indexOf(fromTrack) + limitedTrackDelta);
        const auto &firstSelectedProcessor = Track::findFirstSelectedProcessor(fromTrack); // valid since lastSelected is valid
        int maxAllowedSlot = tracks.getNumSlotsForTrack(toTrack) - 1;
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
    if (multipleTracksWithSelections && Track::hasSelections(tracks.getMasterTrack()))
        return {0, 0};

    bool anyTrackSelected = tracks.anyTrackSelected();

    // When dragging from a non-master track to the master track, interpret as dragging beyond the y-limit,
    // to whatever track slot corresponding to the master track x-position (x/y is flipped in master track).
    if (multipleTracksWithSelections &&
        !Track::isMaster(tracks.getTrack(fromTrackAndSlot.x)) &&
        Track::isMaster(tracks.getTrack(toTrackAndSlot.x))) {
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
        const auto &fromTrack = tracks.getTrack(fromTrackIndex);
        const int toTrackIndex = fromTrackIndex + trackAndSlotDelta.x;

        if (Track::isSelected(fromTrack)) {
            if (fromTrackIndex != toTrackIndex) {
                insertActions.add(new InsertTrackAction(fromTrackIndex, toTrackIndex, tracks));
                insertActions.getLast()->perform();
            }
            return;
        }

        if (!Track::findFirstSelectedProcessor(fromTrack).isValid()) return;

        const auto selectedProcessors = Track::findSelectedProcessors(fromTrack);
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
        for (int trackIndex = 0; trackIndex < tracks.getNumTracks(); trackIndex++)
            addInsertsForTrackIndex(trackIndex);
    } else {
        for (int trackIndex = tracks.getNumTracks() - 1; trackIndex >= 0; trackIndex--)
            addInsertsForTrackIndex(trackIndex);
    }

    return insertActions;
}

MoveSelectedItems::MoveSelectionsAction::MoveSelectionsAction(juce::Point<int> trackAndSlotDelta, Tracks &tracks, Connections &connections, View &view, Input &input,
                                                              ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, processorGraph) {
    if (trackAndSlotDelta.y != 0) {
        for (int i = 0; i < tracks.getNumTracks(); i++) {
            if (oldTrackSelections.getUnchecked(i))
                continue; // track itself is being moved, so don't move its selected slots
            const auto &track = tracks.getTrack(i);
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
