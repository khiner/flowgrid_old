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
    ValueTree source(ConnectionSourceIDs::SOURCE);
    source.setProperty(ProcessorIDs::nodeId, int(connection.source.nodeID.uid), nullptr);
    source.setProperty(ConnectionIDs::channel, connection.source.channelIndex, nullptr);
    connectionState.appendChild(source, nullptr);

    ValueTree destination(ConnectionDestinationIDs::DESTINATION);
    destination.setProperty(ProcessorIDs::nodeId, int(connection.destination.nodeID.uid), nullptr);
    destination.setProperty(ConnectionIDs::channel, connection.destination.channelIndex, nullptr);
    connectionState.appendChild(destination, nullptr);

    if (!isDefault) {
        connectionState.setProperty(ConnectionIDs::isCustomConnection, true, nullptr);
    }

    return connectionState;
}
