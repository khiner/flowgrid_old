#include "DisconnectProcessor.h"

DisconnectProcessor::DisconnectProcessor(Connections &connections, const ValueTree &processor, ConnectionType connectionType, bool defaults, bool custom, bool incoming, bool outgoing,
                                         AudioProcessorGraph::NodeID excludingRemovalTo)
        : CreateOrDeleteConnections(connections) {
    const auto nodeConnections = connections.getConnectionsForNode(processor, connectionType, incoming, outgoing);
    for (const auto &connection : nodeConnections) {
        const auto destinationNodeId = Processor::getNodeId(connection.getChildWithName(ConnectionDestinationIDs::DESTINATION));
        if (excludingRemovalTo != destinationNodeId) {
            coalesceWith(DeleteConnection(connection, custom, defaults, connections));
        }
    }
}
