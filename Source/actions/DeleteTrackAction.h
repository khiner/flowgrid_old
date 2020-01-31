#pragma once

#include <state/ConnectionsStateManager.h>
#include <state/TracksStateManager.h>
#include <actions/DeleteProcessorAction.h>

#include "JuceHeader.h"

struct DeleteTrackAction : public UndoableAction {
    DeleteTrackAction(const ValueTree& trackToDelete, TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager)
            : trackToDelete(trackToDelete), trackIndex(trackToDelete.getParent().indexOf(trackToDelete)),
              tracksManager(tracksManager), connectionsManager(connectionsManager) {
        while (trackToDelete.getNumChildren() > 0) {
            const ValueTree &processorToDelete = trackToDelete.getChild(trackToDelete.getNumChildren() - 1);
            deleteProcessorActions.add(new DeleteProcessorAction(processorToDelete, tracksManager, connectionsManager));
        }
    }

    bool perform() override {
        for (auto* deleteProcessorAction : deleteProcessorActions) {
            deleteProcessorAction->perform();
        }
        tracksManager.getState().removeChild(trackIndex, nullptr);

        return true;
    }

    bool undo() override {
        tracksManager.getState().addChild(trackToDelete, trackIndex, nullptr);
        for (auto* deleteProcessorAction : deleteProcessorActions) {
            deleteProcessorAction->undo();
        }

        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree trackToDelete;
    int trackIndex;

    TracksStateManager &tracksManager;
    ConnectionsStateManager &connectionsManager;

    OwnedArray<DeleteProcessorAction> deleteProcessorActions;

    JUCE_DECLARE_NON_COPYABLE(DeleteTrackAction)
};
