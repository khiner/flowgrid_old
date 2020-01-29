#pragma once

#include <state_managers/ConnectionsStateManager.h>
#include <state_managers/TracksStateManager.h>
#include <actions/FocusAction.h>

#include "JuceHeader.h"

struct SelectAction : public UndoableAction {
    SelectAction(TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager)
            : tracksManager(tracksManager), connectionsManager(connectionsManager), viewManager(viewManager) {
        this->oldSelectedSlotsMasks = tracksManager.getSelectedSlotsMasks();
        this->oldTrackSelections = tracksManager.getTrackSelections();
        this->oldFocusedSlot = viewManager.getFocusedTrackAndSlot();
        this->newSelectedSlotsMasks = oldSelectedSlotsMasks;
        this->newTrackSelections = oldTrackSelections;
        this->newFocusedSlot = oldFocusedSlot;
    }

    SelectAction(TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager,
                 Array<String> oldSelectedSlotsMasks, Array<bool> oldTrackSelections, juce::Point<int> oldFocusedSlot,
                 Array<String> newSelectedSlotsMasks, Array<bool> newTrackSelections, juce::Point<int> newFocusedSlot)
            : tracksManager(tracksManager), connectionsManager(connectionsManager), viewManager(viewManager),
              oldSelectedSlotsMasks(std::move(oldSelectedSlotsMasks)), newSelectedSlotsMasks(std::move(newSelectedSlotsMasks)),
              oldTrackSelections(std::move(oldTrackSelections)), newTrackSelections(std::move(newTrackSelections)),
              oldFocusedSlot(oldFocusedSlot), newFocusedSlot(newFocusedSlot) {
        jassert(this->oldTrackSelections.size() == this->newTrackSelections.size());
        jassert(this->newTrackSelections.size() == this->newSelectedSlotsMasks.size());
        jassert(this->newSelectedSlotsMasks.size() == this->tracksManager.getNumTracks());
    }

    bool perform() override {
        for (int i = 0; i < newTrackSelections.size(); i++) {
            auto track = tracksManager.getTrack(i);
            track.setProperty(IDs::selected, newTrackSelections.getUnchecked(i), nullptr);
            track.setProperty(IDs::selectedSlotsMask, newSelectedSlotsMasks.getUnchecked(i), nullptr);
        }
        viewManager.focusOnProcessorSlot(newFocusedSlot);
        connectionsManager.resetDefaultExternalInputs(nullptr);

        return true;
    }

    bool undo() override {
        for (int i = 0; i < oldTrackSelections.size(); i++) {
            auto track = tracksManager.getTrack(i);
            track.setProperty(IDs::selected, oldTrackSelections.getUnchecked(i), nullptr);
            track.setProperty(IDs::selectedSlotsMask, oldSelectedSlotsMasks.getUnchecked(i), nullptr);
        }
        viewManager.focusOnProcessorSlot(oldFocusedSlot);
        connectionsManager.resetDefaultExternalInputs(nullptr);

        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    UndoableAction* createCoalescedAction(UndoableAction* nextAction) override {
        if (auto* nextSelect = dynamic_cast<SelectAction*>(nextAction)) {
            if (canCoalesceWith(nextSelect)) {
                return new SelectAction(tracksManager, connectionsManager, viewManager,
                                        oldSelectedSlotsMasks, oldTrackSelections, oldFocusedSlot,
                                        nextSelect->newSelectedSlotsMasks, nextSelect->newTrackSelections, nextSelect->newFocusedSlot);
            }
        } else if (auto* nextFocus = dynamic_cast<FocusAction*>(nextAction)) {
            return new SelectAction(tracksManager, connectionsManager, viewManager,
                                    oldSelectedSlotsMasks, oldTrackSelections, oldFocusedSlot,
                                    newSelectedSlotsMasks, newTrackSelections, nextFocus->newFocusedSlot);
        }

        return nullptr;
    }

protected:
    bool canCoalesceWith(SelectAction *otherAction) {
        return oldSelectedSlotsMasks.size() == otherAction->oldSelectedSlotsMasks.size() &&
               oldTrackSelections.size() == otherAction->oldTrackSelections.size();
    }

    TracksStateManager &tracksManager;
    ConnectionsStateManager &connectionsManager;
    ViewStateManager &viewManager;
    Array<String> oldSelectedSlotsMasks, newSelectedSlotsMasks;
    Array<bool> oldTrackSelections, newTrackSelections;
    juce::Point<int> oldFocusedSlot, newFocusedSlot;

private:
    JUCE_DECLARE_NON_COPYABLE(SelectAction)
};
