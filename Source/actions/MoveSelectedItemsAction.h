#pragma once

#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

struct MoveSelectedItemsAction : UndoableAction {
    MoveSelectedItemsAction(juce::Point<int> fromTrackAndSlot, juce::Point<int> toTrackAndSlot, bool makeInvalidDefaultsIntoCustom,
                            TracksState &tracks, ConnectionsState &connections, ViewState &view,
                            InputState &input, OutputState &output, StatefulAudioProcessorContainer &audioProcessorContainer);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(juce::Point<int> trackAndSlotDelta,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer);
    };

    struct InsertTrackAction : UndoableAction {
        InsertTrackAction(int fromTrackIndex, int toTrackIndex, TracksState &tracks);

        bool perform() override;

        bool undo() override;

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
    OwnedArray<UndoableAction> createInsertActions(TracksState &tracks, ViewState &view) const;
};
