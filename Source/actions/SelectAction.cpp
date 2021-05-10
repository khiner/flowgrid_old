#include "SelectAction.h"

SelectAction::SelectAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
        : tracks(tracks), connections(connections), view(view),
          input(input), audioProcessorContainer(audioProcessorContainer) {
    this->oldSelectedSlotsMasks = tracks.getSelectedSlotsMasks();
    this->oldTrackSelections = tracks.getTrackSelections();
    this->oldFocusedSlot = view.getFocusedTrackAndSlot();
    this->newSelectedSlotsMasks = oldSelectedSlotsMasks;
    this->newTrackSelections = oldTrackSelections;
    this->newFocusedSlot = oldFocusedSlot;
}

SelectAction::SelectAction(SelectAction *coalesceLeft, SelectAction *coalesceRight, TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                           StatefulAudioProcessorContainer &audioProcessorContainer)
        : tracks(tracks), connections(connections), view(view),
          input(input), audioProcessorContainer(audioProcessorContainer),
          oldSelectedSlotsMasks(std::move(coalesceLeft->oldSelectedSlotsMasks)), newSelectedSlotsMasks(std::move(coalesceRight->newSelectedSlotsMasks)),
          oldTrackSelections(std::move(coalesceLeft->oldTrackSelections)), newTrackSelections(std::move(coalesceRight->newTrackSelections)),
          oldFocusedSlot(coalesceLeft->oldFocusedSlot), newFocusedSlot(coalesceRight->newFocusedSlot) {
    jassert(this->oldTrackSelections.size() == this->newTrackSelections.size());
    jassert(this->newTrackSelections.size() == this->newSelectedSlotsMasks.size());
    jassert(this->newSelectedSlotsMasks.size() == this->tracks.getNumTracks());

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

void SelectAction::setNewFocusedSlot(juce::Point<int> newFocusedSlot, bool resetDefaultExternalInputs) {
    this->newFocusedSlot = newFocusedSlot;
    if (resetDefaultExternalInputs && oldFocusedSlot.x != this->newFocusedSlot.x) {
        resetInputsAction = std::make_unique<ResetDefaultExternalInputConnectionsAction>(connections, tracks, input, audioProcessorContainer, getNewFocusedTrack());
    }
}

bool SelectAction::perform() {
    if (!changed()) return false;

    for (int i = 0; i < newTrackSelections.size(); i++) {
        auto track = tracks.getTrack(i);
        auto lane = TracksState::getProcessorLaneForTrack(track);
        track.setProperty(IDs::selected, newTrackSelections.getUnchecked(i), nullptr);
        lane.setProperty(IDs::selectedSlotsMask, newSelectedSlotsMasks.getUnchecked(i), nullptr);
    }
    if (newFocusedSlot != oldFocusedSlot)
        updateViewFocus(newFocusedSlot);
    if (resetInputsAction != nullptr)
        resetInputsAction->perform();

    return true;
}

bool SelectAction::undo() {
    if (!changed()) return false;

    if (resetInputsAction != nullptr)
        resetInputsAction->undo();
    for (int i = 0; i < oldTrackSelections.size(); i++) {
        auto track = tracks.getTrack(i);
        auto lane = TracksState::getProcessorLaneForTrack(track);
        track.setProperty(IDs::selected, oldTrackSelections.getUnchecked(i), nullptr);
        lane.setProperty(IDs::selectedSlotsMask, oldSelectedSlotsMasks.getUnchecked(i), nullptr);
    }
    if (oldFocusedSlot != newFocusedSlot)
        updateViewFocus(oldFocusedSlot);
    return true;
}

UndoableAction *SelectAction::createCoalescedAction(UndoableAction *nextAction) {
    if (auto *nextSelect = dynamic_cast<SelectAction *>(nextAction))
        if (canCoalesceWith(nextSelect))
            return new SelectAction(this, nextSelect, tracks, connections, view, input, audioProcessorContainer);
    return nullptr;
}

bool SelectAction::canCoalesceWith(SelectAction *otherAction) {
    return oldSelectedSlotsMasks.size() == otherAction->oldSelectedSlotsMasks.size() &&
           oldTrackSelections.size() == otherAction->oldTrackSelections.size();
}

void SelectAction::updateViewFocus(const juce::Point<int> focusedSlot) {
    view.focusOnProcessorSlot(focusedSlot);
    const auto &focusedTrack = tracks.getTrack(focusedSlot.x);
    bool isMaster = TracksState::isMasterTrack(focusedTrack);
    if (!isMaster)
        view.updateViewTrackOffsetToInclude(focusedSlot.x, tracks.getNumNonMasterTracks());
    view.updateViewSlotOffsetToInclude(focusedSlot.y, isMaster);
}

bool SelectAction::changed() {
    if (resetInputsAction != nullptr || oldFocusedSlot != newFocusedSlot) return true;

    for (int i = 0; i < oldSelectedSlotsMasks.size(); i++) {
        if (oldSelectedSlotsMasks.getUnchecked(i) != newSelectedSlotsMasks.getUnchecked(i) ||
            oldTrackSelections.getUnchecked(i) != newTrackSelections.getUnchecked(i))
            return true;
    }
    return false;
}
