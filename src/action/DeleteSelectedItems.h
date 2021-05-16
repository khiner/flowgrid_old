#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"
#include "DeleteTrack.h"

struct DeleteSelectedItems : public UndoableAction {
    DeleteSelectedItems(Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    OwnedArray<DeleteTrack> deleteTrackActions;
    OwnedArray<DeleteProcessor> deleteProcessorActions;
};
