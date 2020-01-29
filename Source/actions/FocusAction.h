#pragma once

#include <state_managers/ConnectionsStateManager.h>
#include <state_managers/TracksStateManager.h>

#include "JuceHeader.h"

struct FocusAction : public UndoableAction {
    FocusAction(const ValueTree& track, int slot,
                TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager)
            : tracksManager(tracksManager), connectionsManager(connectionsManager), viewManager(viewManager) {
        this->oldFocusedSlot = viewManager.getFocusedTrackAndSlot();

        if (slot == -1) {
            const ValueTree &firstProcessor = tracksManager.getTrack(newFocusedSlot.x).getChild(0);
            this->newFocusedSlot.y = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
        }
        this->newFocusedSlot = {tracksManager.indexOf(track), slot};
    }

    FocusAction(TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager, ViewStateManager &viewManager,
                juce::Point<int> oldFocusedSlot, juce::Point<int> newFocusedSlot)
            : oldFocusedSlot(oldFocusedSlot), newFocusedSlot(newFocusedSlot),
              tracksManager(tracksManager), connectionsManager(connectionsManager), viewManager(viewManager) {}

    bool perform() override {
        viewManager.focusOnProcessorSlot(newFocusedSlot);
        connectionsManager.resetDefaultExternalInputs(nullptr);

        return true;
    }

    bool undo() override {
        viewManager.focusOnProcessorSlot(oldFocusedSlot);
        connectionsManager.resetDefaultExternalInputs(nullptr);

        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    UndoableAction* createCoalescedAction(UndoableAction* nextAction) override {
        if (auto* nextFocus = dynamic_cast<FocusAction*>(nextAction)) {
            return new FocusAction(tracksManager, connectionsManager, viewManager, oldFocusedSlot, nextFocus->newFocusedSlot);
        } // TODO should be able to coalesce select/focus both ways, but this creates a circular dependency :(

        return nullptr;
    }
    juce::Point<int> oldFocusedSlot, newFocusedSlot;

protected:
    TracksStateManager &tracksManager;
    ConnectionsStateManager &connectionsManager;
    ViewStateManager &viewManager;

private:
    JUCE_DECLARE_NON_COPYABLE(FocusAction)
};
