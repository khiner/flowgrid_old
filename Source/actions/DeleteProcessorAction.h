#pragma once

#include "state/ConnectionsState.h"
#include "state/TracksState.h"
#include "DisconnectProcessorAction.h"
#include "ProcessorGraph.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    bool performTemporary();

    bool undoTemporary();

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    TracksState &tracks;
    int trackIndex;
    ValueTree processorToDelete;
    int processorIndex, pluginWindowType;
    DisconnectProcessorAction disconnectProcessorAction;
    ProcessorGraph &processorGraph;
};
