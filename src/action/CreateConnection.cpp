#include "CreateConnection.h"

// TODO iterate through state instead of getProcessorStateForNodeId
CreateConnection::CreateConnection(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnections(connections) {
    if (processorGraph.canAddConnection(connection) &&
        (!isDefault || (processorGraph.getProcessorStateForNodeId(connection.source.nodeID)[ProcessorIDs::allowDefaultConnections] &&
                        processorGraph.getProcessorStateForNodeId(connection.destination.nodeID)[ProcessorIDs::allowDefaultConnections]))) {
        addConnection(stateForConnection(connection, isDefault));
    }
}

ValueTree CreateConnection::stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault) {
    ValueTree connectionState(ConnectionIDs::CONNECTION);
    Connection::setSourceNodeId(connectionState, connection.source.nodeID);
    Connection::setSourceChannel(connectionState, connection.source.channelIndex);
    Connection::setDestinationNodeId(connectionState, connection.destination.nodeID);
    Connection::setDestinationChannel(connectionState, connection.destination.channelIndex);
    if (!isDefault) connectionState.setProperty(ConnectionIDs::isCustomConnection, true, nullptr);

    return connectionState;
}
