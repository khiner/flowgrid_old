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
        source.setProperty(IDs::nodeId, int(connection.source.nodeID.uid), nullptr);
        source.setProperty(IDs::channel, connection.source.channelIndex, nullptr);
        connectionState.addChild(source, -1, nullptr);

        ValueTree destination(IDs::DESTINATION);
        destination.setProperty(IDs::nodeId, int(connection.destination.nodeID.uid), nullptr);
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

    // Connect the given processor to the appropriate default external device input.
    void defaultConnect(AudioProcessorGraph::NodeID fromNodeId, AudioProcessorGraph::NodeID toNodeId, ConnectionType connectionType, UndoManager *undoManager) {
        if (toNodeId.isValid()) {
            const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
            for (auto channel : defaultConnectionChannels) {
                addDefaultConnection({{fromNodeId, channel}, {toNodeId, channel}}, undoManager);
            }
        }
    }

    const Array<int>& getDefaultConnectionChannels(ConnectionType connectionType) const {
        return connectionType == audio ? defaultAudioConnectionChannels : defaultMidiConnectionChannels;
    }

    // make a snapshot of all the information needed to capture AudioGraph connections and UI positions
    void makeConnectionsSnapshot() {
        connectionsSnapshot.clear();
        for (const auto& connection : connections) {
            connectionsSnapshot.add(connection.createCopy());
        }
    }

    void restoreConnectionsSnapshot() {
        connections.removeAllChildren(nullptr);
        for (const auto& connection : connectionsSnapshot) {
            connections.addChild(connection, -1, nullptr);
        }
    }

    bool checkedAddConnection(const AudioProcessorGraph::Connection &c, bool isDefault, UndoManager* undoManager) {
        if (isDefault && false/*&& project.isShiftHeld() */)
            return false; // no default connection stuff while shift is held
        if (canConnectUi(c)) {
            if (!isDefault)
                disconnectDefaultOutgoing(c.source.nodeID, c.source.isMIDI() ? midi : audio, undoManager); // gdum
            addConnection(c, undoManager, isDefault);
            return true;
        }
        return false;
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

        return !getConnectionMatching({{source->nodeID, sourceChannel}, {dest->nodeID, destChannel}}).isValid();
    }

    bool checkedRemoveConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        if (!connection.isValid() || (!connection[IDs::isCustomConnection] && false/*&& project.isShiftHeld() */))
            return false; // no default connection stuff while shift is held
        return removeConnection(connection, undoManager, allowDefaults, allowCustom);
    }

    bool checkedRemoveConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool defaults, bool custom) {
        const ValueTree &connectionState = getConnectionMatching(connection);
        return checkedRemoveConnection(connectionState, undoManager, defaults, custom);
    }

    bool doDisconnectNode(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager,
                          bool defaults, bool custom, bool incoming, bool outgoing) {
        const auto connections = getConnectionsForNode(nodeId, connectionType, incoming, outgoing);
        bool anyRemoved = false;
        for (const auto &connection : connections) {
            if (checkedRemoveConnection(connection, undoManager, defaults, custom))
                anyRemoved = true;
        }

        return anyRemoved;
    }

    bool disconnectNode(AudioProcessorGraph::NodeID nodeId, UndoManager *undoManager) { // gdum
        return doDisconnectNode(nodeId, all, undoManager, true, true, true, true);
    }

    bool disconnectDefaults(AudioProcessorGraph::NodeID nodeId, UndoManager *undoManager) { // gdum
        return doDisconnectNode(nodeId, all, undoManager, true, false, true, true);
    }

    bool disconnectDefaultOutgoing(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager) {
        return doDisconnectNode(nodeId, connectionType, undoManager, true, false, false, true);
    }

    bool disconnectCustom(AudioProcessorGraph::NodeID nodeId, UndoManager *undoManager) { // gdum
        return doDisconnectNode(nodeId, all, undoManager, false, true, true, true);
    }

    bool addConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) { // gdum
        return checkedAddConnection(c, false, undoManager);
    }

    bool addDefaultConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        return checkedAddConnection(c, true, undoManager);
    }

    bool addCustomConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        return checkedAddConnection(c, false, undoManager);
    }

    bool removeDefaultConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        return checkedRemoveConnection(c, undoManager, true, false);
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

    void clear() {
        connections.removeAllChildren(nullptr);
    }
private:
    ValueTree connections;
    StatefulAudioProcessorContainer& audioProcessorContainer;

    Array<int> defaultAudioConnectionChannels {0, 1};
    Array<int> defaultMidiConnectionChannels {AudioProcessorGraph::midiChannelIndex};

    Array<ValueTree> connectionsSnapshot;
};
