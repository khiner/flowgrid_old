#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct Select : public UndoableAction {
    Select(Tracks &tracks, Connections &connections, View &view, Input &input, ProcessorGraph &processorGraph);
    Select(Select *coalesceLeft, Select *coalesceRight, Tracks &tracks, Connections &connections, View &view, Input &input, ProcessorGraph &processorGraph);

    void setNewFocusedSlot(juce::Point<int> newFocusedSlot, bool resetDefaultExternalInputs = true);
    ValueTree getNewFocusedTrack() { return tracks.getTrackState(newFocusedSlot.x); }

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    UndoableAction *createCoalescedAction(UndoableAction *nextAction) override;

protected:
    bool canCoalesceWith(Select *otherAction);
    void updateViewFocus(juce::Point<int> focusedSlot);
    bool changed();

    Tracks &tracks;
    Connections &connections;
    View &view;
    Input &input;
    ProcessorGraph &processorGraph;

    Array<BigInteger> oldSelectedSlotsMasks, newSelectedSlotsMasks;
    Array<bool> oldTrackSelections, newTrackSelections;
    juce::Point<int> oldFocusedSlot, newFocusedSlot;

    std::unique_ptr<ResetDefaultExternalInputConnectionsAction> resetInputsAction;
};
