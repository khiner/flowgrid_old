#include <utility>

#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "unordered_map"

using SAPC = StatefulAudioProcessorContainer;

enum ConnectionType { audio, midi, all };

class ConnectionsStateManager : public StateManager {
public:
    explicit ConnectionsStateManager(StatefulAudioProcessorContainer& audioProcessorContainer)
            : audioProcessorContainer(audioProcessorContainer) {
        connections = ValueTree(IDs::CONNECTIONS);
    }

    ValueTree& getState() override { return connections; }

    void loadFromState(const ValueTree& state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);
    }

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

            const auto &endpointType = connection.getChildWithProperty(IDs::nodeId, int(nodeId.uid));
            bool directionIsAcceptable = (incoming && endpointType.hasType(IDs::DESTINATION)) || (outgoing && endpointType.hasType(IDs::SOURCE));
            bool typeIsAcceptable = connectionType == all || (connectionType == audio && int(endpointType[IDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                    (connectionType == midi && int(endpointType[IDs::channel]) == AudioProcessorGraph::midiChannelIndex);

            if (directionIsAcceptable && typeIsAcceptable)
                nodeConnections.add(connection);
        }

        return nodeConnections;
    }

    static AudioProcessorGraph::Connection stateToConnection(const ValueTree& connectionState) {
        const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
        const auto& destState = connectionState.getChildWithName(IDs::DESTINATION);
        return {{SAPC::getNodeIdForState(sourceState), int(sourceState[IDs::channel])}, {SAPC::getNodeIdForState(destState), int(destState[IDs::channel])}};
    }

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : connections) {
            if (stateToConnection(connectionState) == connection) {
                return connectionState;
            }
        }
        return {};
    }

    bool hasConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (const auto& connectionState : connections) {
            if (stateToConnection(connectionState) == connection) {
                return true;
            }
        }
        return false;
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

    bool removeConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        bool customIsAcceptable = (allowCustom && connection.hasProperty(IDs::isCustomConnection)) ||
                                  (allowDefaults && !connection.hasProperty(IDs::isCustomConnection));
        if (customIsAcceptable) {
            connections.removeChild(connection, undoManager);
            return true;
        }
        return false;
    }

    const Array<int>& getDefaultConnectionChannels(ConnectionType connectionType) const {
        return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsSnapshot = connections.createCopy();
    }

    void restoreConnectionsSnapshot() {
        connections.removeAllChildren(nullptr);
        for (const auto& connection : connectionsSnapshot) {
            connections.addChild(connection, -1, nullptr);
        }
    }

    bool canConnectUi(const AudioProcessorGraph::Connection& c) const {
        if (auto* source = audioProcessorContainer.getNodeForId(c.source.nodeID))
            if (auto* dest = audioProcessorContainer.getNodeForId(c.destination.nodeID))
                return canConnectUi(source, c.source.channelIndex, dest, c.destination.channelIndex);

        return false;
    }

    bool canConnectUi(AudioProcessorGraph::Node* source, int sourceChannel, AudioProcessorGraph::Node* dest, int destChannel) const noexcept {
        bool sourceIsMIDI = sourceChannel == AudioProcessorGraph::midiChannelIndex;
        bool destIsMIDI   = destChannel == AudioProcessorGraph::midiChannelIndex;

        if (sourceChannel < 0
            || destChannel < 0
            || source == dest
            || sourceIsMIDI != destIsMIDI)
            return false;

        if (source == nullptr
            || (!sourceIsMIDI && sourceChannel >= source->getProcessor()->getTotalNumOutputChannels())
            || (sourceIsMIDI && ! source->getProcessor()->producesMidi()))
            return false;

        if (dest == nullptr
            || (!destIsMIDI && destChannel >= dest->getProcessor()->getTotalNumInputChannels())
            || (destIsMIDI && ! dest->getProcessor()->acceptsMidi()))
            return false;

        return !hasConnectionMatching({{source->nodeID, sourceChannel}, {dest->nodeID, destChannel}});
    }

    void addConnection(const ValueTree& connection, UndoManager *undoManager) {
        connections.addChild(connection, -1, undoManager);
    }

    bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        return isProcessorAProducer(processor, connectionType) && isProcessorAnEffect(otherProcessor, connectionType);
    }

    bool isProcessorAProducer(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumOutputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->producesMidi());
        }
        return false;
    }

    bool isProcessorAnEffect(const ValueTree &processorState, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            return (connectionType == audio && processorWrapper->processor->getTotalNumInputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->acceptsMidi());
        }
        return false;
    }
private:
    ValueTree connections;
    StatefulAudioProcessorContainer& audioProcessorContainer;

    Array<int> defaultAudioConnectionChannels {0, 1};
    Array<int> defaultMidiConnectionChannels {AudioProcessorGraph::midiChannelIndex};

    ValueTree connectionsSnapshot;
};
