#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct Select : public UndoableAction {
    Select(Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);
    Select(Select *coalesceLeft, Select *coalesceRight, Tracks &, Connections &, View &, Input &, AllProcessors &, ProcessorGraph &);

    void setNewFocusedSlot(juce::Point<int> newFocusedSlot, bool resetDefaultExternalInputs = true);
    Track *getNewFocusedTrack() { return tracks.get(newFocusedSlot.x); }

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
    AllProcessors &allProcessors;
    ProcessorGraph &processorGraph;

    Array<BigInteger> oldSelectedSlotsMasks, newSelectedSlotsMasks;
    Array<bool> oldTrackSelections, newTrackSelections;
    juce::Point<int> oldFocusedSlot, newFocusedSlot;

    std::unique_ptr<ResetDefaultExternalInputConnectionsAction> resetInputsAction;
};
