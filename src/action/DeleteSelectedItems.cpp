#include "DeleteSelectedItems.h"

DeleteSelectedItems::DeleteSelectedItems(Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph) {
    for (auto *selectedTrack : tracks.findAllSelectedTracks()) {
        deleteTrackActions.add(new DeleteTrack(selectedTrack, tracks, connections, processorGraph));
    }
    for (const auto &selectedItem : tracks.findAllSelectedProcessors()) {
        deleteProcessorActions.add(new DeleteProcessor(selectedItem, tracks.getTrackForProcessor(selectedItem), connections, processorGraph));
        deleteProcessorActions.getLast()->performTemporary();
    }
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undoTemporary();
}

bool DeleteSelectedItems::perform() {
    for (auto *deleteProcessorAction : deleteProcessorActions)
        deleteProcessorAction->perform();
    for (auto *deleteTrackAction : deleteTrackActions)
        deleteTrackAction->perform();
    return !deleteProcessorActions.isEmpty() || !deleteTrackActions.isEmpty();
}

bool DeleteSelectedItems::undo() {
    for (int i = deleteTrackActions.size() - 1; i >= 0; i--)
        deleteTrackActions.getUnchecked(i)->undo();
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undo();
    return !deleteProcessorActions.isEmpty() || !deleteTrackActions.isEmpty();
}
