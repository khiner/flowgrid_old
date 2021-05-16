#include "DisconnectProcessorAction.h"

DisconnectProcessorAction::DisconnectProcessorAction(Connections &connections, const ValueTree &processor, ConnectionType connectionType, bool defaults, bool custom, bool incoming, bool outgoing,
                                                     AudioProcessorGraph::NodeID excludingRemovalTo)
        : CreateOrDeleteConnectionsAction(connections) {
    const auto nodeConnections = connections.getConnectionsForNode(processor, connectionType, incoming, outgoing);
    for (const auto &connection : nodeConnections) {
        const auto destinationNodeId = Tracks::getNodeIdForProcessor(connection.getChildWithName(ConnectionsIDs::DESTINATION));
        if (excludingRemovalTo != destinationNodeId) {
            coalesceWith(DeleteConnectionAction(connection, custom, defaults, connections));
        }
    }
}
