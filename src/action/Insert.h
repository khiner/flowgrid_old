#pragma once

#include "Select.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct Insert : UndoableAction {
    Insert(bool duplicate, const ValueTree &copiedTracks, juce::Point<int> toTrackAndSlot,
           Tracks &tracks, Connections &connections, View &view, Input &input, ProcessorGraph &processorGraph);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    Tracks &tracks;
    View &view;
    ProcessorGraph &processorGraph;
    juce::Point<int> fromTrackAndSlot, toTrackAndSlot;
    juce::Point<int> oldFocusedTrackAndSlot, newFocusedTrackAndSlot;
    OwnedArray<UndoableAction> createActions;
    std::unique_ptr<Select> selectAction;

    struct MoveSelections : public Select {
        MoveSelections(const OwnedArray<UndoableAction> &createActions,
                       Tracks &tracks, Connections &connections, View &view,
                       Input &input, ProcessorGraph &processorGraph);
    };

    void duplicateSelectedProcessors(const ValueTree &track, const ValueTree &copiedTracks);

    void copyProcessorsFromTrack(const ValueTree &fromTrack, int fromTrackIndex, int toTrackIndex, int slotDiff);

    void addAndPerformAction(UndoableAction *action);

    void addAndPerformCreateProcessorAction(const ValueTree &processor, int fromTrackIndex, int fromSlot, int toTrackIndex, int toSlot);

    void addAndPerformCreateTrackAction(const ValueTree &track, int fromTrackIndex, int toTrackIndex);
};
