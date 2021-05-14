#include "DisconnectProcessorAction.h"

#include "state/Identifiers.h"

DisconnectProcessorAction::DisconnectProcessorAction(ConnectionsState &connections, const ValueTree &processor, ConnectionType connectionType, bool defaults, bool custom, bool incoming, bool outgoing,
                                                     AudioProcessorGraph::NodeID excludingRemovalTo)
        : CreateOrDeleteConnectionsAction(connections) {
    const auto nodeConnections = connections.getConnectionsForNode(processor, connectionType, incoming, outgoing);
    for (const auto &connection : nodeConnections) {
        const auto destinationNodeId = TracksState::getNodeIdForProcessor(connection.getChildWithName(ConnectionsStateIDs::DESTINATION));
        if (excludingRemovalTo != destinationNodeId) {
            coalesceWith(DeleteConnectionAction(connection, custom, defaults, connections));
        }
    }
}
