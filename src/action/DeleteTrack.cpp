#include "DeleteTrack.h"

DeleteTrack::DeleteTrack(const ValueTree &trackToDelete, Tracks &tracks, Connections &connections, ProcessorGraph &processorGraph)
        : trackToDelete(trackToDelete), trackIndex(trackToDelete.getParent().indexOf(trackToDelete)),
          tracks(tracks) {
    for (const auto &processor : Tracks::getAllProcessorsForTrack(trackToDelete)) {
        deleteProcessorActions.add(new DeleteProcessor(processor, tracks, connections, processorGraph));
        deleteProcessorActions.getLast()->performTemporary();
    }
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--) {
        deleteProcessorActions.getUnchecked(i)->undoTemporary();
    }
}

bool DeleteTrack::perform() {
    for (auto *deleteProcessorAction : deleteProcessorActions)
        deleteProcessorAction->perform();
    tracks.getState().removeChild(trackIndex, nullptr);
    return true;
}

bool DeleteTrack::undo() {
    tracks.getState().addChild(trackToDelete, trackIndex, nullptr);
    for (int i = deleteProcessorActions.size() - 1; i >= 0; i--)
        deleteProcessorActions.getUnchecked(i)->undo();
    return true;
}
