#pragma once

#include "model/Connections.h"
#include "DisconnectProcessor.h"
#include "ProcessorGraph.h"

struct DeleteProcessor : public UndoableAction {
    DeleteProcessor(const ValueTree &processorToDelete, Connections &connections, ProcessorGraph &processorGraph);

    bool perform() override;
    bool undo() override;

    bool performTemporary();
    bool undoTemporary();

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    ValueTree track;
    int trackIndex;
    ValueTree processorToDelete;
    int processorIndex, pluginWindowType;
    DisconnectProcessor disconnectProcessorAction;
    ProcessorGraph &processorGraph;
};
