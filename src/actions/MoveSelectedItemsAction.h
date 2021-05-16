#pragma once

#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

struct MoveSelectedItemsAction : UndoableAction {
    MoveSelectedItemsAction(juce::Point<int> fromTrackAndSlot, juce::Point<int> toTrackAndSlot, bool makeInvalidDefaultsIntoCustom,
                            Tracks &tracks, Connections &connections, View &view,
                            Input &input, Output &output, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(juce::Point<int> trackAndSlotDelta,
                             Tracks &tracks, Connections &connections, View &view,
                             Input &input, ProcessorGraph &processorGraph);
    };

    struct InsertTrackAction : UndoableAction {
        InsertTrackAction(int fromTrackIndex, int toTrackIndex, Tracks &tracks);

        bool perform() override;

        bool undo() override;

        int fromTrackIndex, toTrackIndex;
        Tracks &tracks;
    };

    juce::Point<int> trackAndSlotDelta;
    MoveSelectionsAction updateSelectionAction;
    OwnedArray<UndoableAction> insertTrackOrProcessorActions;
    UpdateAllDefaultConnectionsAction updateConnectionsAction;

    // As a side effect, this method actually performs the processor/track moves in preparation for
    // `createUpdateConnectionsAction`, which should be called immediately after this.
    // This avoids a redundant `undo` on all insert actions here, as well as the subsequent
    // `perform` that would be needed in `createUpdateConnectionsAction` to find the new default connections.
    OwnedArray<UndoableAction> createInsertActions(Tracks &tracks, View &view) const;
};
