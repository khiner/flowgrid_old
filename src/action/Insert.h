#pragma once

#include "Select.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct Insert : UndoableAction {
    Insert(bool duplicate, const OwnedArray<Track> &copiedTracks, juce::Point<int> toTrackAndSlot,
           Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    Tracks &tracks;
    View &view;
    AllProcessors &allProcessors;
    ProcessorGraph &processorGraph;
    juce::Point<int> fromTrackAndSlot, toTrackAndSlot;
    juce::Point<int> oldFocusedTrackAndSlot, newFocusedTrackAndSlot;
    OwnedArray<UndoableAction> createActions;
    std::unique_ptr<Select> selectAction;

    struct MoveSelections : public Select {
        MoveSelections(const OwnedArray<UndoableAction> &createActions, Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);
    };

    void duplicateSelectedProcessors(const Track *track, const OwnedArray<Track> &copiedTracks);
    void copyProcessorsFromTrack(const Track *fromTrack, int fromTrackIndex, int toTrackIndex, int slotDiff);
    void addAndPerformAction(UndoableAction *action);
    void addAndPerformCreateProcessorAction(int fromTrackIndex, int fromSlot, int toTrackIndex, int toSlot);
    void addAndPerformCreateTrackAction(int fromTrackIndex, int toTrackIndex);
};
