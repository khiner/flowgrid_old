#include "DeleteTrackAction.h"

DeleteTrackAction::DeleteTrackAction(const ValueTree &trackToDelete, TracksState &tracks, ConnectionsState &connections, ProcessorGraph &processorGraph)
        : trackToDelete(trackToDelete), trackIndex(trackToDelete.getParent().indexOf(trackToDelete)),
          tracks(tracks) {
    for (const auto &processor : TracksState::getAllProcessorsForTrack(trackToDelete)) {
        deleteProcessorActions.add(new DeleteProcessorAction(processor, tracks, connections, processorGraph));
        deleteProcessorActions.getLast()->performTemporary();
    }
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--) {
        deleteProcessorActions.getUnchecked(i)->undoTemporary();
    }
}

bool DeleteTrackAction::perform() {
    for (auto *deleteProcessorAction : deleteProcessorActions)
        deleteProcessorAction->perform();
    tracks.getState().removeChild(trackIndex, nullptr);
    return true;
}

bool DeleteTrackAction::undo() {
    tracks.getState().addChild(trackToDelete, trackIndex, nullptr);
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undo();
    return true;
}
