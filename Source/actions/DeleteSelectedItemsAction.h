#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"
#include "DeleteTrackAction.h"

struct DeleteSelectedItemsAction : public UndoableAction {
    DeleteSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), connections(connections) {
        for (const auto &selectedItem : tracks.findAllSelectedItems()) {
            if (selectedItem.hasType(IDs::TRACK))
                deleteTrackActions.add(new DeleteTrackAction(selectedItem, tracks, connections, audioProcessorContainer));
            else if (selectedItem.hasType(IDs::PROCESSOR))
                deleteProcessorActions.add(new DeleteProcessorAction(selectedItem, tracks, connections, audioProcessorContainer));
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
    TracksState &tracks;
    ConnectionsState &connections;

    OwnedArray<DeleteTrackAction> deleteTrackActions;
    OwnedArray<DeleteProcessorAction> deleteProcessorActions;

    JUCE_DECLARE_NON_COPYABLE(DeleteSelectedItemsAction)
};
