#pragma once

#include "model/AllProcessors.h"
#include "model/Connections.h"
#include "CreateOrDeleteConnections.h"
#include "ProcessorGraph.h"

struct CreateConnection : public CreateOrDeleteConnections {
    CreateConnection(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, AllProcessors &allProcessors, ProcessorGraph &processorGraph);

    static ValueTree stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault);
};
