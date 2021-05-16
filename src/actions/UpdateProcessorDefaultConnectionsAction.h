#pragma once

#include "model/Output.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "ProcessorGraph.h"

struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            Connections &connections, Output &output, ProcessorGraph &processorGraph);
};
