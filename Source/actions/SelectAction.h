#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct SelectAction : public UndoableAction {
    SelectAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                 StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), connections(connections), view(view),
              input(input), audioProcessorContainer(audioProcessorContainer) {
        this->oldSelectedSlotsMasks = tracks.getSelectedSlotsMasks();
        this->oldTrackSelections = tracks.getTrackSelections();
        this->oldFocusedSlot = view.getFocusedTrackAndSlot();
        this->newSelectedSlotsMasks = oldSelectedSlotsMasks;
        this->newTrackSelections = oldTrackSelections;
        this->newFocusedSlot = oldFocusedSlot;
    }

    SelectAction(TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                 StatefulAudioProcessorContainer &audioProcessorContainer,
                 SelectAction* coalesceLeft, SelectAction* coalesceRight)
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

    void setNewFocusedSlot(juce::Point<int> newFocusedSlot) {
        this->newFocusedSlot = newFocusedSlot;
        if (oldFocusedSlot.x != this->newFocusedSlot.x) {
            resetInputsAction = std::make_unique<ResetDefaultExternalInputConnectionsAction>(true, connections, tracks, input, audioProcessorContainer, tracks.getTrack(this->newFocusedSlot.x));
        }
    }

    bool perform() override {
        for (int i = 0; i < newTrackSelections.size(); i++) {
            auto track = tracks.getTrack(i);
            track.setProperty(IDs::selected, newTrackSelections.getUnchecked(i), nullptr);
            track.setProperty(IDs::selectedSlotsMask, newSelectedSlotsMasks.getUnchecked(i), nullptr);
        }
        view.focusOnProcessorSlot(newFocusedSlot);
        if (resetInputsAction != nullptr)
            resetInputsAction->perform();

        return true;
    }

    bool undo() override {
        if (resetInputsAction != nullptr)
            resetInputsAction->undo();
        for (int i = 0; i < oldTrackSelections.size(); i++) {
            auto track = tracks.getTrack(i);
            track.setProperty(IDs::selected, oldTrackSelections.getUnchecked(i), nullptr);
            track.setProperty(IDs::selectedSlotsMask, oldSelectedSlotsMasks.getUnchecked(i), nullptr);
        }
        view.focusOnProcessorSlot(oldFocusedSlot);

        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    UndoableAction* createCoalescedAction(UndoableAction* nextAction) override {
        if (auto* nextSelect = dynamic_cast<SelectAction*>(nextAction)) {
            if (canCoalesceWith(nextSelect)) {
                return new SelectAction(tracks, connections, view, input, audioProcessorContainer, this, nextSelect);
            }
        }

        return nullptr;
    }

protected:
    bool canCoalesceWith(SelectAction *otherAction) {
        return oldSelectedSlotsMasks.size() == otherAction->oldSelectedSlotsMasks.size() &&
               oldTrackSelections.size() == otherAction->oldTrackSelections.size();
    }

    TracksState &tracks;
    ConnectionsState &connections;
    ViewState &view;
    InputState &input;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    Array<String> oldSelectedSlotsMasks, newSelectedSlotsMasks;
    Array<bool> oldTrackSelections, newTrackSelections;

    std::unique_ptr<ResetDefaultExternalInputConnectionsAction> resetInputsAction;

private:
    juce::Point<int> oldFocusedSlot, newFocusedSlot;

    JUCE_DECLARE_NON_COPYABLE(SelectAction)
};
