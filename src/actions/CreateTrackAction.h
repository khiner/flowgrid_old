#pragma once

#include "state/Connections.h"
#include "state/Tracks.h"

struct CreateTrackAction : public UndoableAction {
    CreateTrackAction(bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view);

    CreateTrackAction(int insertIndex, bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    ValueTree newTrack;
    int insertIndex;

protected:
    Tracks &tracks;
};
