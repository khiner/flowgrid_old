#pragma once

#include "state/InputState.h"
#include "CreateConnectionAction.h"
#include "UpdateProcessorDefaultConnectionsAction.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct UpdateAllDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections,
                                      TracksState &tracks, ConnectionsState &connections, InputState &input, OutputState &output,
                                      ProcessorGraph &processorGraph, ValueTree trackToTreatAsFocused = {});
};
