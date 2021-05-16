#include "CreateConnectionAction.h"

// TODO iterate through state instead of getProcessorStateForNodeId
CreateConnectionAction::CreateConnectionAction(const AudioProcessorGraph::Connection &connection, bool isDefault, Connections &connections, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnectionsAction(connections) {
    if (processorGraph.canAddConnection(connection) &&
        (!isDefault || (processorGraph.getProcessorStateForNodeId(connection.source.nodeID)[ProcessorIDs::allowDefaultConnections] &&
                        processorGraph.getProcessorStateForNodeId(connection.destination.nodeID)[ProcessorIDs::allowDefaultConnections]))) {
        addConnection(stateForConnection(connection, isDefault));
    }
}

ValueTree CreateConnectionAction::stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault) {
    ValueTree connectionState(ConnectionIDs::CONNECTION);
    ValueTree source(ConnectionsIDs::SOURCE);
    source.setProperty(ProcessorIDs::nodeId, int(connection.source.nodeID.uid), nullptr);
    source.setProperty(ConnectionsIDs::channel, connection.source.channelIndex, nullptr);
    connectionState.appendChild(source, nullptr);

    ValueTree destination(ConnectionsIDs::DESTINATION);
    destination.setProperty(ProcessorIDs::nodeId, int(connection.destination.nodeID.uid), nullptr);
    destination.setProperty(ConnectionsIDs::channel, connection.destination.channelIndex, nullptr);
    connectionState.appendChild(destination, nullptr);

    if (!isDefault) {
        connectionState.setProperty(ConnectionsIDs::isCustomConnection, true, nullptr);
    }

    return connectionState;
}
