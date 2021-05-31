#include "Select.h"

Select::Select(Tracks &tracks, Connections &connections, View &view, Input &input, ProcessorGraph &processorGraph)
        : tracks(tracks), connections(connections), view(view),
          input(input), processorGraph(processorGraph) {
    this->oldSelectedSlotsMasks = tracks.getSelectedSlotsMasks();
    this->oldTrackSelections = tracks.getTrackSelections();
    this->oldFocusedSlot = view.getFocusedTrackAndSlot();
    this->newSelectedSlotsMasks = oldSelectedSlotsMasks;
    this->newTrackSelections = oldTrackSelections;
    this->newFocusedSlot = oldFocusedSlot;
}

Select::Select(Select *coalesceLeft, Select *coalesceRight, Tracks &tracks, Connections &connections, View &view, Input &input,
               ProcessorGraph &processorGraph)
        : tracks(tracks), connections(connections), view(view),
          input(input), processorGraph(processorGraph),
          oldSelectedSlotsMasks(std::move(coalesceLeft->oldSelectedSlotsMasks)), newSelectedSlotsMasks(std::move(coalesceRight->newSelectedSlotsMasks)),
          oldTrackSelections(std::move(coalesceLeft->oldTrackSelections)), newTrackSelections(std::move(coalesceRight->newTrackSelections)),
          oldFocusedSlot(coalesceLeft->oldFocusedSlot), newFocusedSlot(coalesceRight->newFocusedSlot) {
    jassert(this->oldTrackSelections.size() == this->newTrackSelections.size());
    jassert(this->newTrackSelections.size() == this->newSelectedSlotsMasks.size());
    jassert(this->newSelectedSlotsMasks.size() == this->tracks.size());

    if (coalesceLeft->resetInputsAction != nullptr) {
        this->resetInputsAction = std::move(coalesceLeft->resetInputsAction);
    }
    if (coalesceRight->resetInputsAction != nullptr) {
        if (this->resetInputsAction == nullptr) {
            this->resetInputsAction = std::move(coalesceRight->resetInputsAction);
        } else {
            this->resetInputsAction->coalesceWith(*(coalesceRight->resetInputsAction.get()));
        }
    }
}

void Select::setNewFocusedSlot(juce::Point<int> newFocusedSlot, bool resetDefaultExternalInputs) {
    this->newFocusedSlot = newFocusedSlot;
    if (resetDefaultExternalInputs && oldFocusedSlot.x != this->newFocusedSlot.x) {
        resetInputsAction = std::make_unique<ResetDefaultExternalInputConnectionsAction>(connections, tracks, input, processorGraph, getNewFocusedTrack());
    }
}

bool Select::perform() {
    if (!changed()) return false;

    for (int i = 0; i < newTrackSelections.size(); i++) {
        auto *track = tracks.getChild(i);
        auto lane = track->getProcessorLane();
        track->setSelected(newTrackSelections.getUnchecked(i));
        ProcessorLane::setSelectedSlotsMask(lane, newSelectedSlotsMasks.getUnchecked(i));
    }
    if (newFocusedSlot != oldFocusedSlot)
        updateViewFocus(newFocusedSlot);
    if (resetInputsAction != nullptr)
        resetInputsAction->perform();

    return true;
}

bool Select::undo() {
    if (!changed()) return false;

    if (resetInputsAction != nullptr)
        resetInputsAction->undo();
    for (int i = 0; i < oldTrackSelections.size(); i++) {
        auto *track = tracks.getChild(i);
        auto lane = track->getProcessorLane();
        track->setSelected(oldTrackSelections.getUnchecked(i));
        ProcessorLane::setSelectedSlotsMask(lane, oldSelectedSlotsMasks.getUnchecked(i));
    }
    if (oldFocusedSlot != newFocusedSlot)
        updateViewFocus(oldFocusedSlot);
    return true;
}

UndoableAction *Select::createCoalescedAction(UndoableAction *nextAction) {
    if (auto *nextSelect = dynamic_cast<Select *>(nextAction))
        if (canCoalesceWith(nextSelect))
            return new Select(this, nextSelect, tracks, connections, view, input, processorGraph);
    return nullptr;
}

bool Select::canCoalesceWith(Select *otherAction) {
    return oldSelectedSlotsMasks.size() == otherAction->oldSelectedSlotsMasks.size() &&
           oldTrackSelections.size() == otherAction->oldTrackSelections.size();
}

void Select::updateViewFocus(const juce::Point<int> focusedSlot) {
    view.focusOnProcessorSlot(focusedSlot);
    const auto *focusedTrack = tracks.getChild(focusedSlot.x);
    bool isMaster = focusedTrack != nullptr && focusedTrack->isMaster();
    if (!isMaster) {
        view.updateViewTrackOffsetToInclude(focusedSlot.x, tracks.getNumNonMasterTracks());
    }
    view.updateViewSlotOffsetToInclude(focusedSlot.y, isMaster);
}

bool Select::changed() {
    if (resetInputsAction != nullptr || oldFocusedSlot != newFocusedSlot) return true;

    for (int i = 0; i < oldSelectedSlotsMasks.size(); i++) {
        if (oldSelectedSlotsMasks.getUnchecked(i) != newSelectedSlotsMasks.getUnchecked(i) ||
            oldTrackSelections.getUnchecked(i) != newTrackSelections.getUnchecked(i))
            return true;
    }
    return false;
}
