#pragma once

#include "state/Input.h"
#include "CreateConnectionAction.h"
#include "UpdateProcessorDefaultConnectionsAction.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct UpdateAllDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    UpdateAllDefaultConnectionsAction(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections,
                                      Tracks &tracks, Connections &connections, Input &input, Output &output,
                                      ProcessorGraph &processorGraph, ValueTree trackToTreatAsFocused = {});
};
