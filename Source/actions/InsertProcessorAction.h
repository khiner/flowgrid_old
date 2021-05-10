#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "state/TracksState.h"
#include "state/ViewState.h"

using namespace juce;

// Inserting a processor "pushes" any contiguous set of processors starting at the given slot down.
// (The new processor always "wins" by keeping its given slot.)
// Doesn't take care of any select actions! (Caller is responsible for that.)
struct InsertProcessorAction : UndoableAction {
    // slot of -1 used for track-level processors (track IO processors that aren't in a lane).
    InsertProcessorAction(const ValueTree &processor, int toTrackIndex, int toSlot, TracksState &tracks, ViewState &view);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    struct SetProcessorSlotAction : public UndoableAction {
        SetProcessorSlotAction(int trackIndex, const ValueTree &processor, int newSlot,
                               TracksState &tracks, ViewState &view);

        bool perform() override;
        bool undo() override;

    private:
        ValueTree processor;
        int oldSlot, newSlot;
        std::unique_ptr<SetProcessorSlotAction> pushConflictingProcessorAction;

        struct AddProcessorRowAction : public UndoableAction {
            AddProcessorRowAction(int trackIndex, TracksState &tracks, ViewState &view);

            bool perform() override;
            bool undo() override;

        private:
            int trackIndex;
            TracksState &tracks;
            ViewState &view;
        };

        OwnedArray<AddProcessorRowAction> addProcessorRowActions;
    };

    struct AddOrMoveProcessorAction : public UndoableAction {
        AddOrMoveProcessorAction(const ValueTree &processor, int newTrackIndex, int newSlot, TracksState &tracks, ViewState &view);

        bool perform() override;
        bool undo() override;

    private:
        ValueTree processor;
        int oldTrackIndex, newTrackIndex;
        int oldSlot, newSlot;
        int oldIndex, newIndex;
        std::unique_ptr<SetProcessorSlotAction> setProcessorSlotAction;
        TracksState &tracks;
    };

    AddOrMoveProcessorAction addOrMoveProcessorAction;
};
