#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"
#include "action/DeleteProcessor.h"

struct DeleteTrack : public UndoableAction {
    DeleteTrack(const ValueTree &trackToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    ValueTree trackToDelete;
    int trackIndex;
    Tracks &tracks;
    OwnedArray<DeleteProcessor> deleteProcessorActions;
};
