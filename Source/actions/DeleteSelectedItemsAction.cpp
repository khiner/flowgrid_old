#include "DeleteSelectedItemsAction.h"

DeleteSelectedItemsAction::DeleteSelectedItemsAction(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer) {
    for (const auto &selectedItem : tracks.findAllSelectedItems()) {
        if (selectedItem.hasType(IDs::TRACK)) {
            deleteTrackActions.add(new DeleteTrackAction(selectedItem, tracks, connections, audioProcessorContainer));
        } else if (selectedItem.hasType(IDs::PROCESSOR)) {
            deleteProcessorActions.add(new DeleteProcessorAction(selectedItem, tracks, connections, audioProcessorContainer));
            deleteProcessorActions.getLast()->performTemporary();
        }
    }
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undoTemporary();
}

bool DeleteSelectedItemsAction::perform() {
    for (auto *deleteProcessorAction : deleteProcessorActions)
        deleteProcessorAction->perform();
    for (auto *deleteTrackAction : deleteTrackActions)
        deleteTrackAction->perform();
    return !deleteProcessorActions.isEmpty() || !deleteTrackActions.isEmpty();
}

bool DeleteSelectedItemsAction::undo() {
    for (int i = deleteTrackActions.size() - 1; i >= 0; i--)
        deleteTrackActions.getUnchecked(i)->undo();
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undo();
    return !deleteProcessorActions.isEmpty() || !deleteTrackActions.isEmpty();
}
