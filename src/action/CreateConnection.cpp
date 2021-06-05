#include "CreateConnection.h"

// TODO iterate through state instead of getProcessorForNodeId
CreateConnection::CreateConnection(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnections(connections) {
    const auto &processorWrappers = processorGraph.getProcessorWrappers();
    if (processorGraph.canAddConnection(connection) &&
        (!isDefault || (processorWrappers.getProcessorForNodeId(connection.source.nodeID)->allowsDefaultConnections() &&
                        processorWrappers.getProcessorForNodeId(connection.destination.nodeID)->allowsDefaultConnections()))) {
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
