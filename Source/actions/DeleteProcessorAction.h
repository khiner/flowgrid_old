#pragma once

#include <state/ConnectionsStateManager.h>
#include <state/TracksStateManager.h>

#include "JuceHeader.h"
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree& processorToDelete,
                          TracksStateManager &tracksManager, ConnectionsStateManager &connectionsManager)
            : parentTrack(processorToDelete.getParent()), processorToDelete(processorToDelete),
              processorIndex(parentTrack.indexOf(processorToDelete)),
              tracksManager(tracksManager), connectionsManager(connectionsManager),
              disconnectProcessorAction(DisconnectProcessorAction(connectionsManager, processorToDelete, all, true, true, true, true)) {
    }

    bool perform() override {
        disconnectProcessorAction.perform();
        parentTrack.removeChild(processorToDelete, nullptr);

        return true;
    }

    bool undo() override {
        parentTrack.addChild(processorToDelete, processorIndex, nullptr);
        disconnectProcessorAction.undo();

        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree parentTrack;
    ValueTree processorToDelete;
    int processorIndex;

    TracksStateManager &tracksManager;
    ConnectionsStateManager &connectionsManager;

    DisconnectProcessorAction disconnectProcessorAction;

    JUCE_DECLARE_NON_COPYABLE(DeleteProcessorAction)
};
