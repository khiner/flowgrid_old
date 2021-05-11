#pragma once

#include "state/ConnectionsState.h"
#include "state/TracksState.h"
#include "actions/DeleteProcessorAction.h"

struct DeleteTrackAction : public UndoableAction {
    DeleteTrackAction(const ValueTree &trackToDelete, TracksState &tracks, ConnectionsState &connections,
                      StatefulAudioProcessorContainer &audioProcessorContainer);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    ValueTree trackToDelete;
    int trackIndex;
    TracksState &tracks;
    OwnedArray<DeleteProcessorAction> deleteProcessorActions;
};
