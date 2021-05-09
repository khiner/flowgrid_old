#pragma once

#include <state/Identifiers.h>
#include <state/ConnectionsState.h>
#include "CreateOrDeleteConnectionsAction.h"

struct CreateConnectionAction : public CreateOrDeleteConnectionsAction {
    CreateConnectionAction(const AudioProcessorGraph::Connection &connection, bool isDefault,
                           ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connections) {
        if (canAddConnection(connection, audioProcessorContainer) &&
            (!isDefault || (audioProcessorContainer.getProcessorStateForNodeId(connection.source.nodeID)[IDs::allowDefaultConnections] &&
                            audioProcessorContainer.getProcessorStateForNodeId(connection.destination.nodeID)[IDs::allowDefaultConnections]))) {
            addConnection(stateForConnection(connection, isDefault));
        }
    }

    bool canAddConnection(const AudioProcessorGraph::Connection &c, StatefulAudioProcessorContainer &audioProcessorContainer) {
        if (auto *source = audioProcessorContainer.getNodeForId(c.source.nodeID))
            if (auto *dest = audioProcessorContainer.getNodeForId(c.destination.nodeID))
                return canAddConnection(source, c.source.channelIndex, dest, c.destination.channelIndex);

        return false;
    }

    bool canAddConnection(AudioProcessorGraph::Node *source, int sourceChannel, AudioProcessorGraph::Node *dest, int destChannel) noexcept {
        bool sourceIsMIDI = sourceChannel == AudioProcessorGraph::midiChannelIndex;
        bool destIsMIDI = destChannel == AudioProcessorGraph::midiChannelIndex;

        if (sourceChannel < 0
            || destChannel < 0
            || source == dest
            || sourceIsMIDI != destIsMIDI)
            return false;

        if (source == nullptr
            || (!sourceIsMIDI && sourceChannel >= source->getProcessor()->getTotalNumOutputChannels())
            || (sourceIsMIDI && !source->getProcessor()->producesMidi()))
            return false;

        if (dest == nullptr
            || (!destIsMIDI && destChannel >= dest->getProcessor()->getTotalNumInputChannels())
            || (destIsMIDI && !dest->getProcessor()->acceptsMidi()))
            return false;

        return !hasConnectionMatching({{source->nodeID, sourceChannel},
                                       {dest->nodeID,   destChannel}});
    }

    bool hasConnectionMatching(const AudioProcessorGraph::Connection &connection) {
        for (const auto &connectionState : connections.getState()) {
            if (ConnectionsState::stateToConnection(connectionState) == connection) {
                return true;
            }
        }
        return false;
    }

    static ValueTree stateForConnection(const AudioProcessorGraph::Connection &connection, bool isDefault) {
        ValueTree connectionState(IDs::CONNECTION);
        ValueTree source(IDs::SOURCE);
        source.setProperty(IDs::nodeId, int(connection.source.nodeID.uid), nullptr);
        source.setProperty(IDs::channel, connection.source.channelIndex, nullptr);
        connectionState.appendChild(source, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::nodeId, int(connection.destination.nodeID.uid), nullptr);
        destination.setProperty(IDs::channel, connection.destination.channelIndex, nullptr);
        connectionState.appendChild(destination, nullptr);

        if (!isDefault) {
            connectionState.setProperty(IDs::isCustomConnection, true, nullptr);
        }

        return connectionState;
    }
};
