#pragma once

#include "state/ConnectionsState.h"
#include "state/TracksState.h"
#include "DeleteTrackAction.h"

struct DeleteSelectedItemsAction : public UndoableAction {
    DeleteSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    OwnedArray<DeleteTrackAction> deleteTrackActions;
    OwnedArray<DeleteProcessorAction> deleteProcessorActions;
};
