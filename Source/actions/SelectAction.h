#pragma once

#include "state/ConnectionsState.h"
#include "state/TracksState.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct SelectAction : public UndoableAction {
    SelectAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                 StatefulAudioProcessorContainer &audioProcessorContainer);

    SelectAction(SelectAction *coalesceLeft, SelectAction *coalesceRight,
                 TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                 StatefulAudioProcessorContainer &audioProcessorContainer);

    void setNewFocusedSlot(juce::Point<int> newFocusedSlot, bool resetDefaultExternalInputs = true);

    ValueTree getNewFocusedTrack() { return tracks.getTrack(newFocusedSlot.x); }

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    UndoableAction *createCoalescedAction(UndoableAction *nextAction) override;

protected:
    bool canCoalesceWith(SelectAction *otherAction);

    void updateViewFocus(juce::Point<int> focusedSlot);

    bool changed();

    TracksState &tracks;
    ConnectionsState &connections;
    ViewState &view;
    InputState &input;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    Array<String> oldSelectedSlotsMasks, newSelectedSlotsMasks;
    Array<bool> oldTrackSelections, newTrackSelections;
    juce::Point<int> oldFocusedSlot, newFocusedSlot;

    std::unique_ptr<ResetDefaultExternalInputConnectionsAction> resetInputsAction;
};
