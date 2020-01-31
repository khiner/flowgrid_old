#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree& processorToDelete, TracksState &tracks, ConnectionsState &connections)
            : parentTrack(processorToDelete.getParent()), processorToDelete(processorToDelete),
              processorIndex(parentTrack.indexOf(processorToDelete)),
              tracks(tracks), connections(connections),
              disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)) {
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

    TracksState &tracks;
    ConnectionsState &connections;

    DisconnectProcessorAction disconnectProcessorAction;

    JUCE_DECLARE_NON_COPYABLE(DeleteProcessorAction)
};
