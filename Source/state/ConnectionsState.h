#pragma once

#include "JuceHeader.h"

#include "StatefulAudioProcessorContainer.h"
#include "TracksState.h"

using SAPC = StatefulAudioProcessorContainer;

class ConnectionsState : public Stateful {
public:
    explicit ConnectionsState(StatefulAudioProcessorContainer &audioProcessorContainer, TracksState &tracks)
            : audioProcessorContainer(audioProcessorContainer), tracks(tracks) {
        connections = ValueTree(IDs::CONNECTIONS);
    }

    ValueTree &getState() override { return connections; }

    void loadFromState(const ValueTree &state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);
    }

    ValueTree findProcessorToFlowInto(const ValueTree &track, const ValueTree &processor, ConnectionType connectionType) {
        if (!isProcessorAProducer(processor, connectionType))
            return {};

        int siblingDelta = 0;
        ValueTree otherTrack;
        while ((otherTrack = track.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherTrack) {
                if (otherProcessor == processor) continue;
                bool isOtherProcessorBelow = int(otherProcessor[IDs::processorSlot]) > int(processor[IDs::processorSlot]) ||
                                             (track != otherTrack && TracksState::isMasterTrack(otherTrack));
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

        return {};
    }

    bool isNodeConnected(AudioProcessorGraph::NodeID nodeId) const {
        for (const auto &connection : connections)
            if (SAPC::getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == nodeId)
                return true;

        return false;
    }

    Array<ValueTree> getConnectionsForNode(const ValueTree &processor, ConnectionType connectionType,
                                           bool incoming = true, bool outgoing = true,
                                           bool includeCustom = true, bool includeDefault = true) {
        Array<ValueTree> nodeConnections;
        for (const auto &connection : connections) {
            if ((connection[IDs::isCustomConnection] && !includeCustom) || (!connection[IDs::isCustomConnection] && !includeDefault))
                continue;

            int processorNodeId = int(StatefulAudioProcessorContainer::getNodeIdForState(processor).uid);
            const auto &endpointType = connection.getChildWithProperty(IDs::nodeId, processorNodeId);
            bool directionIsAcceptable = (incoming && endpointType.hasType(IDs::DESTINATION)) || (outgoing && endpointType.hasType(IDs::SOURCE));
            bool typeIsAcceptable = connectionType == all ||
                                    (connectionType == audio && int(endpointType[IDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                    (connectionType == midi && int(endpointType[IDs::channel]) == AudioProcessorGraph::midiChannelIndex);

            if (directionIsAcceptable && typeIsAcceptable)
                nodeConnections.add(connection);
        }

        return nodeConnections;
    }

    static AudioProcessorGraph::Connection stateToConnection(const ValueTree &connectionState) {
        const auto &sourceState = connectionState.getChildWithName(IDs::SOURCE);
        const auto &destState = connectionState.getChildWithName(IDs::DESTINATION);
        return {{SAPC::getNodeIdForState(sourceState), int(sourceState[IDs::channel])},
                {SAPC::getNodeIdForState(destState),   int(destState[IDs::channel])}};
    }

    ValueTree getConnectionMatching(const AudioProcessorGraph::Connection &connection) const {
        for (auto connectionState : connections)
            if (stateToConnection(connectionState) == connection)
                return connectionState;
        return {};
    }

    bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) const {
        if (!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor)
            return false;

        return isProcessorAProducer(processor, connectionType) && isProcessorAnEffect(otherProcessor, connectionType);
    }

    bool isProcessorAProducer(const ValueTree &processor, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processor))
            return (connectionType == audio && processorWrapper->processor->getTotalNumOutputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->producesMidi());
        return false;
    }

    bool isProcessorAnEffect(const ValueTree &processor, ConnectionType connectionType) const {
        if (auto *processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processor))
            return (connectionType == audio && processorWrapper->processor->getTotalNumInputChannels() > 0) ||
                   (connectionType == midi && processorWrapper->processor->acceptsMidi());
        return false;
    }

    ValueTree findFirstProcessorReceivingDefaultConnectionsFrom(AudioProcessorGraph::NodeID sourceNodeId, ConnectionType connectionType) {
        const auto outgoingConnections = getConnectionsForNode(audioProcessorContainer.getProcessorStateForNodeId(sourceNodeId), connectionType, false, true, false, true);
        if (outgoingConnections.isEmpty())
            return {};
        return audioProcessorContainer.getProcessorStateForNodeId(SAPC::getNodeIdForState(outgoingConnections.getFirst().getChildWithName(IDs::DESTINATION)));
    }

private:
    ValueTree connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    TracksState &tracks;
};
