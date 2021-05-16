#pragma once

#include "state/OutputState.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "ProcessorGraph.h"

struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            ConnectionsState &connections, OutputState &output, ProcessorGraph &processorGraph);
};
