#pragma once

#include "model/Input.h"
#include "CreateConnection.h"
#include "UpdateProcessorDefaultConnections.h"
#include "ResetDefaultExternalInputConnectionsAction.h"

struct UpdateAllDefaultConnections : public CreateOrDeleteConnections {
    UpdateAllDefaultConnections(bool makeInvalidDefaultsIntoCustom, bool resetDefaultExternalInputConnections,
                                Tracks &tracks, Connections &connections, Input &input, Output &output,
                                AllProcessors &allProcessors, ProcessorGraph &processorGraph, Track *trackToTreatAsFocused = nullptr);
};
