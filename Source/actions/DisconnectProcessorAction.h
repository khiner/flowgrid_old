#pragma once

#include "state/Identifiers.h"
#include "actions/DeleteConnectionAction.h"
#include "StatefulAudioProcessorContainer.h"

struct DisconnectProcessorAction : public CreateOrDeleteConnectionsAction {
    DisconnectProcessorAction(ConnectionsState &connections, const ValueTree &processor, ConnectionType connectionType,
                              bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {})
            : CreateOrDeleteConnectionsAction(connections) {
        const auto nodeConnections = connections.getConnectionsForNode(processor, connectionType, incoming, outgoing);
        for (const auto &connection : nodeConnections) {
            const auto destinationNodeId = StatefulAudioProcessorContainer::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
            if (excludingRemovalTo != destinationNodeId) {
                coalesceWith(DeleteConnectionAction(connection, custom, defaults, connections));
            }
        }
    }
};
