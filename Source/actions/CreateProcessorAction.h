#pragma once

#include "state/TracksState.h"
#include "InsertProcessorAction.h"
#include "ProcessorGraph.h"

struct CreateProcessorAction : public UndoableAction {
    CreateProcessorAction(ValueTree processorToCreate, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, ProcessorGraph &processorGraph);

    CreateProcessorAction(const PluginDescription &description, int trackIndex, int slot,
                          TracksState &tracks, ViewState &view, ProcessorGraph &processorGraph);

    CreateProcessorAction(const PluginDescription &description, int trackIndex,
                          TracksState &tracks, ViewState &view, ProcessorGraph &processorGraph);

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
    InsertProcessorAction insertAction;
    ProcessorGraph &processorGraph;
};
