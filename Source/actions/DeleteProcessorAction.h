#pragma once

#include "state/ConnectionsState.h"
#include "state/TracksState.h"
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections,
                          StatefulAudioProcessorContainer &audioProcessorContainer);

    bool perform() override;
    bool undo() override;

    bool performTemporary();
    bool undoTemporary();

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    TracksState &tracks;
    int trackIndex;
    ValueTree processorToDelete;
    int processorIndex, pluginWindowType;
    DisconnectProcessorAction disconnectProcessorAction;
    StatefulAudioProcessorContainer &audioProcessorContainer;
};
