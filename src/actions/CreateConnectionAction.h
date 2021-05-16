#pragma once

#include "state/ConnectionsState.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "ProcessorGraph.h"

struct CreateConnectionAction : public CreateOrDeleteConnectionsAction {
    CreateConnectionAction(const AudioProcessorGraph::Connection &connection, bool isDefault, ConnectionsState &connections, ProcessorGraph &processorGraph);

    static ValueTree stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault);
};
