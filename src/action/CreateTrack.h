#pragma once

#include "model/Connections.h"
#include "model/Tracks.h"

struct CreateTrack : public UndoableAction {
    CreateTrack(bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view);
    CreateTrack(int insertIndex, bool isMaster, const ValueTree &derivedFromTrack, Tracks &tracks, View &view);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    ValueTree newTrack;
    int insertIndex;

protected:
    Tracks &tracks;
};
