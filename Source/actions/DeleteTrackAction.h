#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>
#include <actions/DeleteProcessorAction.h>

#include "JuceHeader.h"

struct DeleteTrackAction : public UndoableAction {
    DeleteTrackAction(const ValueTree& trackToDelete, TracksState &tracks, ConnectionsState &connections,
                      StatefulAudioProcessorContainer &audioProcessorContainer)
            : trackToDelete(trackToDelete), trackIndex(trackToDelete.getParent().indexOf(trackToDelete)),
              tracks(tracks), connections(connections) {
        while (trackToDelete.getNumChildren() > 0) {
            const ValueTree &processorToDelete = trackToDelete.getChild(trackToDelete.getNumChildren() - 1);
            deleteProcessorActions.add(new DeleteProcessorAction(processorToDelete, tracks, connections, audioProcessorContainer));
        }
    }

    bool perform() override {
        for (auto* deleteProcessorAction : deleteProcessorActions) {
            deleteProcessorAction->perform();
        }
        tracks.getState().removeChild(trackIndex, nullptr);

        return true;
    }

    bool undo() override {
        tracks.getState().addChild(trackToDelete, trackIndex, nullptr);
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

    TracksState &tracks;
    ConnectionsState &connections;

    OwnedArray<DeleteProcessorAction> deleteProcessorActions;

    JUCE_DECLARE_NON_COPYABLE(DeleteTrackAction)
};
