#pragma once

#include "SelectAction.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct InsertAction : UndoableAction {
    InsertAction(bool duplicate, const ValueTree &copiedState, juce::Point<int> toTrackAndSlot,
                 TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                 StatefulAudioProcessorContainer &audioProcessorContainer);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    TracksState &tracks;
    ViewState &view;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    juce::Point<int> fromTrackAndSlot, toTrackAndSlot;
    juce::Point<int> oldFocusedTrackAndSlot, newFocusedTrackAndSlot;
    OwnedArray<UndoableAction> createActions;
    std::unique_ptr<SelectAction> selectAction;

    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(const OwnedArray<UndoableAction> &createActions,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer);
    };

    void duplicateSelectedProcessors(const ValueTree &track, const ValueTree &copiedState);
    void copyProcessorsFromTrack(const ValueTree &fromTrack, int fromTrackIndex, int toTrackIndex, int slotDiff);
    void addAndPerformAction(UndoableAction *action);
    void addAndPerformCreateProcessorAction(const ValueTree &processor, int fromTrackIndex, int fromSlot, int toTrackIndex, int toSlot);
    void addAndPerformCreateTrackAction(const ValueTree &track, int fromTrackIndex, int toTrackIndex);
};
