#include <utility>

#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "TracksStateManager.h"

using SAPC = StatefulAudioProcessorContainer;

class ConnectionsStateManager : public StateManager {
public:
    explicit ConnectionsStateManager(StatefulAudioProcessorContainer& audioProcessorContainer,
                                     InputStateManager& inputManager, OutputStateManager& outputManager,
                                     TracksStateManager& tracksManager)
            : audioProcessorContainer(audioProcessorContainer),
              inputManager(inputManager),
              outputManager(outputManager),
              tracksManager(tracksManager) {
        connections = ValueTree(IDs::CONNECTIONS);
    }

    ValueTree& getState() override { return connections; }

    void loadFromState(const ValueTree& state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);
    }


    bool checkedRemoveConnection(const AudioProcessorGraph::Connection &connection, UndoManager* undoManager, bool defaults, bool custom) {
        const ValueTree &connectionState = getConnectionMatching(connection);
        return checkedRemoveConnection(connectionState, undoManager, defaults, custom);
    }

    bool doDisconnectNode(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager,
                          bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        const auto connections = getConnectionsForNode(nodeId, connectionType, incoming, outgoing);
        bool anyRemoved = false;
        for (const auto &connection : connections) {
            if (excludingRemovalTo != SAPC::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION)) &&
                checkedRemoveConnection(connection, undoManager, defaults, custom))
                anyRemoved = true;
        }

        return anyRemoved;
    }

    bool removeConnection(const AudioProcessorGraph::Connection& c, UndoManager *undoManager) {
        const ValueTree &connectionState = getConnectionMatching(c);
        bool removed = checkedRemoveConnection(c, undoManager, true, true);
        if (removed && connectionState.hasProperty(IDs::isCustomConnection)) {
            const auto& sourceState = connectionState.getChildWithName(IDs::SOURCE);
            auto nodeId = StatefulAudioProcessorContainer::getNodeIdForState(sourceState);
            const auto& processor = audioProcessorContainer.getProcessorStateForNodeId(nodeId);
            updateDefaultConnectionsForProcessor(processor, undoManager);
            resetDefaultExternalInputs(undoManager);
        }
        return removed;
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
        addConnection(connectionState, undoManager);
        if (!isDefault) {
            const auto& processor = audioProcessorContainer.getProcessorStateForNodeId(SAPC::getNodeIdForState(source));
            updateDefaultConnectionsForProcessor(processor, undoManager);
            resetDefaultExternalInputs(undoManager);
        }
    }

    bool checkedAddConnection(const AudioProcessorGraph::Connection &c, bool isDefault, UndoManager* undoManager) {
        if (isDefault && false/*isShiftHeld()*/)
            return false; // TODO no default connection stuff while shift is held
        if (canConnectUi(c)) {
            if (!isDefault)
                disconnectDefaultOutgoing(c.source.nodeID, c.source.isMIDI() ? midi : audio, undoManager);
            addConnection(c, undoManager, isDefault);

            return true;
        }
        return false;
    }

    bool checkedRemoveConnection(const ValueTree& connection, UndoManager* undoManager, bool allowDefaults, bool allowCustom) {
        if (!connection.isValid() || (!connection[IDs::isCustomConnection] && false/*&& isShiftHeld() */))
            return false; // TODO no default connection stuff while shift is held
        return removeConnection(connection, undoManager, allowDefaults, allowCustom);
    }

    ValueTree findProcessorToFlowInto(const ValueTree &track, const ValueTree &processor, ConnectionType connectionType) {
        if (!processor.hasType(IDs::PROCESSOR) || !isProcessorAProducer(processor, connectionType))
            return {};

        int siblingDelta = 0;
        ValueTree otherTrack;
        while ((otherTrack = track.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherTrack) {
                if (otherProcessor == processor) continue;
                bool isOtherProcessorBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]) ||
                                             (track != otherTrack && TracksStateManager::isMasterTrack(otherTrack));
                if (!isOtherProcessorBelow) continue;
                if (canProcessorDefaultConnectTo(processor, otherProcessor, connectionType) ||
                    // If a non-effect (another producer) is under this processor in the same track, and no effect processors
                    // to the right have a lower slot, block this producer's output by the other producer,
                    // effectively replacing it.
                    track == otherTrack) {
                    return otherProcessor;
                }
            }
        }

        return outputManager.getAudioOutputProcessorState();
    }

    bool disconnectProcessor(const ValueTree& processor, UndoManager *undoManager) {
        return disconnectNode(SAPC::getNodeIdForState(processor), undoManager);
    }

    bool disconnectNode(AudioProcessorGraph::NodeID nodeId, UndoManager *undoManager) {
        return doDisconnectNode(nodeId, all, undoManager, true, true, true, true);
    }

    bool disconnectDefaultOutgoing(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType, UndoManager* undoManager, AudioProcessorGraph::NodeID excludingRemovalTo={}) {
        return doDisconnectNode(nodeId, connectionType, undoManager, true, false, false, true, excludingRemovalTo);
    }

    bool disconnectCustom(const ValueTree& processor, UndoManager *undoManager) {
        return doDisconnectNode(SAPC::getNodeIdForState(processor), all, undoManager, false, true, true, true);
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

    void updateAllDefaultConnections(UndoManager *undoManager, bool makeInvalidDefaultsIntoCustom=false) {
        for (const auto& track : tracksManager.getState()) {
            for (const auto& processor : track) {
                updateDefaultConnectionsForProcessor(processor, undoManager, makeInvalidDefaultsIntoCustom);
            }
        }
        resetDefaultExternalInputs(undoManager);
    }

    void updateDefaultConnectionsForProcessor(const ValueTree &processor, UndoManager *undoManager, bool makeInvalidDefaultsIntoCustom=false) {
        for (auto connectionType : {audio, midi}) {
            AudioProcessorGraph::NodeID nodeId = StatefulAudioProcessorContainer::getNodeIdForState(processor);
            if (!processor[IDs::allowDefaultConnections]) {
                doDisconnectNode(nodeId, all, undoManager, true, false, true, true);
                return;
            }
            auto outgoingCustomConnections = getConnectionsForNode(nodeId, connectionType, false, true, true, false);
            if (!outgoingCustomConnections.isEmpty()) {
                disconnectDefaultOutgoing(nodeId, connectionType, undoManager);
                return;
            }
            auto outgoingDefaultConnections = getConnectionsForNode(nodeId, connectionType, false, true, false, true);
            auto processorToConnectTo = findProcessorToFlowInto(processor.getParent(), processor, connectionType);
            auto nodeIdToConnectTo = StatefulAudioProcessorContainer::getNodeIdForState(processorToConnectTo);

            const auto &defaultChannels = getDefaultConnectionChannels(connectionType);
            bool anyCustomAdded = false;
            for (const auto &connection : outgoingDefaultConnections) {
                AudioProcessorGraph::NodeID nodeIdCurrentlyConnectedTo = StatefulAudioProcessorContainer::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                if (nodeIdCurrentlyConnectedTo != nodeIdToConnectTo) {
                    for (auto channel : defaultChannels) {
                        removeDefaultConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}, undoManager);
                    }
                    if (makeInvalidDefaultsIntoCustom) {
                        for (auto channel : defaultChannels) {
                            if (addCustomConnection({{nodeId, channel}, {nodeIdCurrentlyConnectedTo, channel}}, undoManager))
                                anyCustomAdded = true;
                        }
                    }
                }
            }
            if (!anyCustomAdded) {
                if (processor[IDs::allowDefaultConnections] && processorToConnectTo[IDs::allowDefaultConnections]) {
                    for (auto channel : defaultChannels) {
                        addDefaultConnection({{nodeId, channel}, {nodeIdToConnectTo, channel}}, undoManager);
                    }
                }
            }
        }
    }

    // Disconnect external audio/midi inputs (unless `addDefaultConnections` is true and
    // the default connection would stay the same).
    // If `addDefaultConnections` is true, then for both audio and midi connection types:
    //   * Find the topmost effect processor (receiving audio/midi) in the focused track
    //   * Connect external device inputs to its most-upstream connected processor (including itself)
    // (Note that it is possible for the same focused track to have a default audio-input processor different
    // from its default midi-input processor.)
    void resetDefaultExternalInputs(UndoManager *undoManager, bool addDefaultConnections=true) {
        const auto audioSourceNodeId = inputManager.getDefaultInputNodeIdForConnectionType(audio);
        const auto midiSourceNodeId = inputManager.getDefaultInputNodeIdForConnectionType(midi);

        AudioProcessorGraph::NodeID audioDestinationNodeId, midiDestinationNodeId;
        if (addDefaultConnections) {
            audioDestinationNodeId = SAPC::getNodeIdForState(findEffectProcessorToReceiveDefaultExternalInput(audio));
            midiDestinationNodeId = SAPC::getNodeIdForState(findEffectProcessorToReceiveDefaultExternalInput(midi));
        }

        defaultConnect(audioSourceNodeId, audioDestinationNodeId, audio, undoManager);
        disconnectDefaultOutgoing(audioSourceNodeId, audio, undoManager, audioDestinationNodeId);

        defaultConnect(midiSourceNodeId, midiDestinationNodeId, midi, undoManager);
        disconnectDefaultOutgoing(midiSourceNodeId, midi, undoManager, midiDestinationNodeId);
    }

    ValueTree findEffectProcessorToReceiveDefaultExternalInput(ConnectionType connectionType) {
        const ValueTree &focusedTrack = tracksManager.getFocusedTrack();
        const ValueTree &topmostEffectProcessor = findTopmostEffectProcessor(focusedTrack, connectionType);
        return findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType);
    }

    ValueTree findTopmostEffectProcessor(const ValueTree& track, ConnectionType connectionType) {
        for (const auto& processor : track) {
            if (isProcessorAnEffect(processor, connectionType)) {
                return processor;
            }
        }
        return {};
    }

private:
    ValueTree connections;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    InputStateManager& inputManager;
    OutputStateManager& outputManager;
    TracksStateManager& tracksManager;

    Array<int> defaultAudioConnectionChannels {0, 1};
    Array<int> defaultMidiConnectionChannels {AudioProcessorGraph::midiChannelIndex};

    ValueTree connectionsSnapshot;

    // Connect the given processor to the appropriate default external device input.
    bool defaultConnect(AudioProcessorGraph::NodeID fromNodeId, AudioProcessorGraph::NodeID toNodeId, ConnectionType connectionType, UndoManager *undoManager) {
        if (fromNodeId.isValid() && toNodeId.isValid()) {
            const auto &defaultConnectionChannels = getDefaultConnectionChannels(connectionType);
            bool anyAdded = false;
            for (auto channel : defaultConnectionChannels) {
                if (addDefaultConnection({{fromNodeId, channel}, {toNodeId, channel}}, undoManager))
                    anyAdded = true;
            }
            return anyAdded;
        }
        return false;
    }

    // Find the upper-right-most processor that flows into the given processor
    // which doesn't already have incoming node connections.
    ValueTree findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType) {
        if (!processor.isValid())
            return {};

        int lowestSlot = INT_MAX;
        ValueTree upperRightMostProcessor;
        AudioProcessorGraph::NodeID processorNodeId = SAPC::getNodeIdForState(processor);
        if (isAvailableForExternalInput(processorNodeId, connectionType))
            upperRightMostProcessor = processor;

        // TODO performance improvement: only iterate over connected processors
        for (int i = tracksManager.getNumTracks() - 1; i >= 0; i--) {
            const auto& track = tracksManager.getTrack(i);
            if (track.getNumChildren() == 0)
                continue;

            const auto& firstProcessor = track.getChild(0);
            auto firstProcessorNodeId = SAPC::getNodeIdForState(firstProcessor);
            int slot = firstProcessor[IDs::processorSlot];
            if (slot < lowestSlot &&
                isAvailableForExternalInput(firstProcessorNodeId, connectionType) &&
                areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

                lowestSlot = slot;
                upperRightMostProcessor = firstProcessor;
            }
        }

        return upperRightMostProcessor;
    }

    bool isAvailableForExternalInput(AudioProcessorGraph::NodeID nodeId, ConnectionType connectionType) const {
        const auto& incomingConnections = getConnectionsForNode(nodeId, connectionType, true, false);
        const auto defaultInputNodeId = inputManager.getDefaultInputNodeIdForConnectionType(connectionType);
        for (const auto& incomingConnection : incomingConnections) {
            if (SAPC::getNodeIdForState(incomingConnection.getChildWithName(IDs::SOURCE)) != defaultInputNodeId) {
                return false;
            }
        }
        return true;
    }
};
