#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"
#include "DisconnectProcessorAction.h"
#include "ProcessorGraph.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    bool performTemporary();

    bool undoTemporary();

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    Tracks &tracks;
    int trackIndex;
    ValueTree processorToDelete;
    int processorIndex, pluginWindowType;
    DisconnectProcessorAction disconnectProcessorAction;
    ProcessorGraph &processorGraph;
};
