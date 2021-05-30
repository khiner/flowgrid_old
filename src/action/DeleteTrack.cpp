#include "DeleteTrack.h"

DeleteTrack::DeleteTrack(Track *trackToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : deletedTrackState(trackToDelete->getState()), trackIndex(trackToDelete->getIndex()), tracks(tracks) {
    for (const auto &processorState : trackToDelete->getAllProcessors()) {
        deleteProcessorActions.add(new DeleteProcessor(processorState, tracks.getTrackForProcessor(processorState), connections, processorGraph));
        deleteProcessorActions.getLast()->performTemporary();
    }
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--) {
        deleteProcessorActions.getUnchecked(i)->undoTemporary();
    }
}

bool DeleteTrack::perform() {
    for (auto *deleteProcessorAction : deleteProcessorActions)
        deleteProcessorAction->perform();
    tracks.remove(trackIndex);
    return true;
}

bool DeleteTrack::undo() {
    tracks.add(deletedTrackState, trackIndex);
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undo();
    return true;
}
