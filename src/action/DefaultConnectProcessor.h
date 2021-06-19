#pragma once

#include "model/AllProcessors.h"
#include "CreateOrDeleteConnections.h"
#include "ProcessorGraph.h"

struct DefaultConnectProcessor : public CreateOrDeleteConnections {
    DefaultConnectProcessor(const Processor *fromProcessor, AudioProcessorGraph::NodeID toNodeId, ConnectionType connectionType, Connections &connections, AllProcessors &allProcessors, ProcessorGraph &processorGraph);
};
