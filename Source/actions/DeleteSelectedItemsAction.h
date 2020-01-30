#pragma once

#include <state_managers/ConnectionsStateManager.h>
#include <state_managers/TracksStateManager.h>

#include "JuceHeader.h"
#include "DeleteTrackAction.h"

struct DeleteSelectedItemsAction : public UndoableAction {
    DeleteSelectedItemsAction(TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager)
            : tracksManager(tracksManager), connectionsManager(connectionsManager) {
        for (const auto &selectedItem : tracksManager.findAllSelectedItems()) {
            if (selectedItem.hasType(IDs::TRACK))
                deleteTrackActions.add(new DeleteTrackAction(selectedItem, tracksManager, connectionsManager));
            else if (selectedItem.hasType(IDs::PROCESSOR))
                deleteProcessorActions.add(new DeleteProcessorAction(selectedItem, tracksManager, connectionsManager));
        }
    }

    bool perform() override {
        for (auto *deleteProcessorAction : deleteProcessorActions)
            deleteProcessorAction->perform();
        for (auto *deleteTrackAction : deleteTrackActions)
            deleteTrackAction->perform();

        return true;
    }

    bool undo() override {
        for (auto *deleteTrackAction : deleteTrackActions)
            deleteTrackAction->undo();
        for (auto *deleteProcessorAction : deleteProcessorActions)
            deleteProcessorAction->undo();

        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    TracksStateManager &tracksManager;
    ConnectionsStateManager &connectionsManager;

    OwnedArray<DeleteTrackAction> deleteTrackActions;
    OwnedArray<DeleteProcessorAction> deleteProcessorActions;

    JUCE_DECLARE_NON_COPYABLE(DeleteSelectedItemsAction)
};
