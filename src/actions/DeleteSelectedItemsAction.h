#pragma once

#include "state/Connections.h"
#include "state/Tracks.h"
#include "DeleteTrackAction.h"

struct DeleteSelectedItemsAction : public UndoableAction {
    DeleteSelectedItemsAction(Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    OwnedArray<DeleteTrackAction> deleteTrackActions;
    OwnedArray<DeleteProcessorAction> deleteProcessorActions;
};
