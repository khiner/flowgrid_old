#pragma once

#include "CreateOrDeleteConnectionsAction.h"
#include "ProcessorGraph.h"

struct DefaultConnectProcessorAction : public CreateOrDeleteConnectionsAction {
    DefaultConnectProcessorAction(const ValueTree &fromProcessor, AudioProcessorGraph::NodeID toNodeId,
                                  ConnectionType connectionType, Connections &connections, ProcessorGraph &processorGraph);
};
