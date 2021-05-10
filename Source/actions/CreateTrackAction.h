#pragma once

#include "state/ConnectionsState.h"
#include "state/TracksState.h"

struct CreateTrackAction : public UndoableAction {
    CreateTrackAction(bool isMaster, const ValueTree &derivedFromTrack, TracksState &tracks, ViewState &view);
    CreateTrackAction(int insertIndex, bool isMaster, const ValueTree& derivedFromTrack, TracksState &tracks, ViewState &view);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    ValueTree newTrack;
    int insertIndex;

protected:
    TracksState &tracks;
};
