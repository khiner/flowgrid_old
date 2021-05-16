#pragma once

#include "model/Tracks.h"
#include "InsertProcessor.h"
#include "ProcessorGraph.h"

struct CreateProcessor : public UndoableAction {
    CreateProcessor(ValueTree processorToCreate, int trackIndex, int slot,
                    Tracks &tracks, View &view, ProcessorGraph &processorGraph);

    CreateProcessor(const PluginDescription &description, int trackIndex, int slot,
                    Tracks &tracks, View &view, ProcessorGraph &processorGraph);

    CreateProcessor(const PluginDescription &description, int trackIndex,
                    Tracks &tracks, View &view, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    // The temporary versions of the perform/undo methods are used only to change the grid state
    // in the creation of other undoable actions that affect the grid.
    bool performTemporary() { return insertAction.perform(); }

    bool undoTemporary() { return insertAction.undo(); }

    int getSizeInUnits() override { return (int) sizeof(*this); }

    static ValueTree createProcessor(const PluginDescription &description);

    int trackIndex, slot;
private:
    ValueTree processorToCreate;
    int pluginWindowType;
    InsertProcessor insertAction;
    ProcessorGraph &processorGraph;
};
