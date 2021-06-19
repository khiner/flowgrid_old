#pragma once

#include "model/Tracks.h"
#include "model/Connections.h"
#include "DisconnectProcessor.h"
#include "ProcessorGraph.h"

struct DeleteProcessor : public UndoableAction {
    DeleteProcessor(Processor *processor, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;
    bool undo() override;

    bool performTemporary(bool apply=false);
    bool undoTemporary(bool apply=false);

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    int trackIndex, processorSlot, processorIndex, pluginWindowType;
    bool graphIsFrozen;
    ValueTree processorState;
    DisconnectProcessor disconnectProcessorAction;
    Tracks &tracks;
    ProcessorGraph &processorGraph;
};
