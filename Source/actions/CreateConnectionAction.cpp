#include "CreateConnectionAction.h"

// TODO iterate through state instead of getProcessorStateForNodeId
CreateConnectionAction::CreateConnectionAction(const AudioProcessorGraph::Connection &connection, bool isDefault, ConnectionsState &connections, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnectionsAction(connections) {
    if (processorGraph.canAddConnection(connection) &&
        (!isDefault || (processorGraph.getProcessorStateForNodeId(connection.source.nodeID)[TracksStateIDs::allowDefaultConnections] &&
                        processorGraph.getProcessorStateForNodeId(connection.destination.nodeID)[TracksStateIDs::allowDefaultConnections]))) {
        addConnection(stateForConnection(connection, isDefault));
    }
}

ValueTree CreateConnectionAction::stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault) {
    ValueTree connectionState(ConnectionStateIDs::CONNECTION);
    ValueTree source(ConnectionsStateIDs::SOURCE);
    source.setProperty(TracksStateIDs::nodeId, int(connection.source.nodeID.uid), nullptr);
    source.setProperty(ConnectionsStateIDs::channel, connection.source.channelIndex, nullptr);
    connectionState.appendChild(source, nullptr);

    ValueTree destination(ConnectionsStateIDs::DESTINATION);
    destination.setProperty(TracksStateIDs::nodeId, int(connection.destination.nodeID.uid), nullptr);
    destination.setProperty(ConnectionsStateIDs::channel, connection.destination.channelIndex, nullptr);
    connectionState.appendChild(destination, nullptr);

    if (!isDefault) {
        connectionState.setProperty(ConnectionsStateIDs::isCustomConnection, true, nullptr);
    }

    return connectionState;
}
