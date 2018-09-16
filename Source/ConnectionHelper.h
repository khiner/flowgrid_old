#include <utility>

#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "unordered_map"

using SAPC = StatefulAudioProcessorContainer;

enum ConnectionType { audio, midi, all };

class ConnectionHelper {
public:
    explicit ConnectionHelper(StatefulAudioProcessorContainer& audioProcessorContainer)
            : audioProcessorContainer(audioProcessorContainer) {
        connections = ValueTree(IDs::CONNECTIONS);
    }

    ValueTree& getState() { return connections; }

    bool isNodeConnected(AudioProcessorGraph::NodeID nodeId) const {
        for (const auto& connection : connections) {
            if (SAPC::getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == nodeId)
                return true;
        }

        return false;
    }

    Array<ValueTree> getConnectionsForNode(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType,
                                           bool incoming=true, bool outgoing=true,
                                           bool includeCustom=true, bool includeDefault=true) const {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections) {
            if ((connection[IDs::isCustomConnection] && !includeCustom) || (!connection[IDs::isCustomConnection] && !includeDefault))
                continue;

            const auto &endpointType = connection.getChildWithProperty(IDs::nodeId, int(nodeId));
            bool directionIsAcceptable = (incoming && endpointType.hasType(IDs::DESTINATION)) || (outgoing && endpointType.hasType(IDs::SOURCE));
            bool typeIsAcceptable = connectionType == all || (connectionType == audio && int(endpointType[IDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                    (connectionType == midi && int(endpointType[IDs::channel]) == AudioProcessorGraph::midiChannelIndex);

            if (directionIsAcceptable && typeIsAcceptable)
                nodeConnections.add(connection);
        }

        return nodeConnections;
    }

    const ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : connections) {
            auto sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto destState = connectionState.getChildWithName(IDs::DESTINATION);
            if (SAPC::getNodeIdForState(sourceState) == connection.source.nodeID &&
                SAPC::getNodeIdForState(destState) == connection.destination.nodeID &&
                int(sourceState[IDs::channel]) == connection.source.channelIndex &&
                int(destState[IDs::channel]) == connection.destination.channelIndex) {
                return connectionState;
            }
        }
        return {};
    }

    bool areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) const {
        if (upstreamNodeId == downstreamNodeId)
            return true;

        Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
        for (const auto& connection : connections) {
            if (SAPC::getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == upstreamNodeId) {
                auto otherDownstreamNodeId = SAPC::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                if (!exploredDownstreamNodes.contains(otherDownstreamNodeId)) {
                    if (otherDownstreamNodeId == downstreamNodeId)
                        return true;
                    else if (areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId))
                        return true;
                    exploredDownstreamNodes.add(otherDownstreamNodeId);
                }
            }
        }
        return false;
    }

    bool nodeHasIncomingConnections(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType) const {
        return !getConnectionsForNode(nodeId, connectionType, true, false).isEmpty();
    }

    // checks for duplicate add should be done before! (not done here to avoid redundant checks)
    void addConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool isDefault=true) {
        ValueTree connectionState(IDs::CONNECTION);
        ValueTree source(IDs::SOURCE);
        source.setProperty(IDs::nodeId, int(connection.source.nodeID), nullptr);
        source.setProperty(IDs::channel, connection.source.channelIndex, nullptr);
        connectionState.addChild(source, -1, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::nodeId, int(connection.destination.nodeID), nullptr);
        destination.setProperty(IDs::channel, connection.destination.channelIndex, nullptr);
        connectionState.addChild(destination, -1, nullptr);

        if (!isDefault) {
            connectionState.setProperty(IDs::isCustomConnection, true, nullptr);
        }
        connections.addChild(connectionState, -1, undoManager);
    }

    bool removeConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        bool customIsAcceptable = (allowCustom && connection.hasProperty(IDs::isCustomConnection)) ||
                                  (allowDefaults && !connection.hasProperty(IDs::isCustomConnection));
        if (customIsAcceptable) {
            connections.removeChild(connection, undoManager);
            return true;
        }
        return false;
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsSnapshot.clear();
        for (auto connection : connections) {
            connectionsSnapshot.add(connection.createCopy());
        }
    }

    void restoreConnectionsSnapshot() {
        connections.removeAllChildren(nullptr);
        for (const auto& connection : connectionsSnapshot) {
            connections.addChild(connection, -1, nullptr);
        }
    }

    void clear() {
        connections.removeAllChildren(nullptr);
    }
private:
    ValueTree connections;
    StatefulAudioProcessorContainer& audioProcessorContainer;

    Array<ValueTree> connectionsSnapshot;
};
