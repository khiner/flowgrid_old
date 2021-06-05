#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"

struct CreateTrack : public UndoableAction {
    CreateTrack(bool isMaster, int derivedFromTrackIndex, Tracks &tracks, View &view);
    CreateTrack(int insertIndex, bool isMaster, int derivedFromTrackIndex, Tracks &tracks, View &view);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    int insertIndex;
    int derivedFromTrackIndex;
    bool isMaster;
protected:
    Tracks &tracks;
};
