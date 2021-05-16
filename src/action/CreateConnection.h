#pragma once

#include "model/Connections.h"
#include "CreateOrDeleteConnections.h"
#include "ProcessorGraph.h"

struct CreateConnection : public CreateOrDeleteConnections {
    CreateConnection(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, ProcessorGraph &processorGraph);

    static ValueTree stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault);
};
