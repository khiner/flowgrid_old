#pragma once

#include "JuceHeader.h"

#include <Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <actions/DeleteConnectionAction.h>

struct DisconnectProcessorAction : public CreateOrDeleteConnectionsAction {
    DisconnectProcessorAction(ConnectionsStateManager &connectionsManager, const ValueTree &processor, ConnectionType connectionType,
                              bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {})
                              : CreateOrDeleteConnectionsAction(connectionsManager) {
        const auto nodeConnections = getConnectionsForNode(processor, connectionType, incoming, outgoing);
        for (const auto &connection : nodeConnections) {
            const auto destinationNodeId = StatefulAudioProcessorContainer::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
            if (excludingRemovalTo != destinationNodeId) {
                coalesceWith(DeleteConnectionAction(connection, custom, defaults, connectionsManager));
            }
        }
    }
private:
    JUCE_DECLARE_NON_COPYABLE(DisconnectProcessorAction)
};