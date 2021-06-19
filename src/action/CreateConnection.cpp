#include "CreateConnection.h"

// TODO iterate through state instead of getProcessorForNodeId
CreateConnection::CreateConnection(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnections(connections) {
    if (processorGraph.canAddConnection(connection) &&
        (!isDefault || (allProcessors.getProcessorByNodeId(connection.source.nodeID)->allowsDefaultConnections() &&
                        allProcessors.getProcessorByNodeId(connection.destination.nodeID)->allowsDefaultConnections()))) {
        addConnection(stateForConnection(connection, isDefault));
    }
}

ValueTree CreateConnection::stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault) {
    ValueTree connectionState(ConnectionIDs::CONNECTION);
    Connection::setSourceNodeId(connectionState, connection.source.nodeID);
    Connection::setSourceChannel(connectionState, connection.source.channelIndex);
    Connection::setDestinationNodeId(connectionState, connection.destination.nodeID);
    Connection::setDestinationChannel(connectionState, connection.destination.channelIndex);
    if (!isDefault) Connection::setCustom(connectionState, true);

    return connectionState;
}
