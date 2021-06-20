#pragma once

#include "model/AllProcessors.h"
#include "InsertProcessor.h"
#include "ProcessorGraph.h"

struct CreateProcessor : public UndoableAction {
    // Copy
    CreateProcessor(juce::Point<int> derivedFromTrackAndSlot, int trackIndex, int slot, Tracks &, View &, AllProcessors &, ProcessorGraph &);
    // Insert in lane
    CreateProcessor(const PluginDescription &, int trackIndex, int slot, Tracks &, View &, AllProcessors &, ProcessorGraph &);
    // Track IO
    CreateProcessor(const PluginDescription &, int trackIndex, Tracks &, View &, AllProcessors &, ProcessorGraph &);
    // Global IO
    CreateProcessor(const PluginDescription &, AllProcessors &, ProcessorGraph &);

    bool perform() override;
    bool undo() override;

    // The temporary versions of the perform/undo methods are used only to change the grid state
    // in the creation of other undoable actions that affect the grid.
    bool performTemporary() {
        if (insertAction != nullptr) return insertAction->perform();
        allProcessors.appendIOProcessor(description);
        return true;
    }
    bool undoTemporary() {
        if (insertAction != nullptr) return insertAction->undo();
        allProcessors.removeIOProcessor(description);
        return true;
    }

    int getSizeInUnits() override { return (int) sizeof(*this); }

    int trackIndex = -1, slot = -1;
private:
    Processor *createdProcessor{};
    int pluginWindowType;
    std::unique_ptr<InsertProcessor> insertAction;
    const PluginDescription description{};
    AllProcessors &allProcessors;
    ProcessorGraph &processorGraph;
};
