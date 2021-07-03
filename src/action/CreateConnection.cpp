#include "CreateConnection.h"

// TODO iterate through state instead of getProcessorForNodeId
CreateConnection::CreateConnection(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnections(connections) {
    if (processorGraph.canAddConnection(connection) &&
        (!isDefault || (allProcessors.getProcessorByNodeId(connection.source.nodeID)->allowsDefaultConnections() &&
                        allProcessors.getProcessorByNodeId(connection.destination.nodeID)->allowsDefaultConnections()))) {
        addConnection(connection, isDefault);
    }
}
