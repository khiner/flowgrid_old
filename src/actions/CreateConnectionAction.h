#pragma once

#include "model/Connections.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "ProcessorGraph.h"

struct CreateConnectionAction : public CreateOrDeleteConnectionsAction {
    CreateConnectionAction(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, ProcessorGraph &processorGraph);

    static ValueTree stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault);
};
