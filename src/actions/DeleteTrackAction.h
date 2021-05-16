#pragma once

#include "state/Connections.h"
#include "state/Tracks.h"
#include "actions/DeleteProcessorAction.h"

struct DeleteTrackAction : public UndoableAction {
    DeleteTrackAction(const ValueTree &trackToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    ValueTree trackToDelete;
    int trackIndex;
    Tracks &tracks;
    OwnedArray<DeleteProcessorAction> deleteProcessorActions;
};
