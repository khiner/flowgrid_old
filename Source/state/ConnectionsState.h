#pragma once

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

    ValueTree findDefaultDestinationProcessor(const ValueTree &sourceProcessor, ConnectionType connectionType) {
        if (!isProcessorAProducer(sourceProcessor, connectionType))
            return {};

        const ValueTree &track = TracksState::getTrackForProcessor(sourceProcessor);
        const auto &masterTrack = tracks.getMasterTrack();
        if (TracksState::isTrackOutputProcessor(sourceProcessor)) {
            if (track == masterTrack)
                return {};
            if (masterTrack.isValid())
                return TracksState::getInputProcessorForTrack(masterTrack);
            return {};
        }

        bool isTrackInputProcessor = TracksState::isTrackInputProcessor(sourceProcessor);
        const auto &lane = TracksState::getProcessorLaneForProcessor(sourceProcessor);
        int siblingDelta = 0;
        ValueTree otherLane;
        while ((otherLane = lane.getSibling(siblingDelta++)).isValid()) {
            for (const auto &otherProcessor : otherLane) {
                if (otherProcessor == sourceProcessor) continue;
                bool isOtherProcessorBelow = isTrackInputProcessor || int(otherProcessor[IDs::processorSlot]) > int(sourceProcessor[IDs::processorSlot]);
                if (!isOtherProcessorBelow) continue;
                if (canProcessorDefaultConnectTo(sourceProcessor, otherProcessor, connectionType) ||
                    // If a non-effect (another producer) is under this processor in the same track, and no effect processors
                    // to the right have a lower slot, block this producer's output by the other producer,
                    // effectively replacing it.
                    lane == otherLane) {
                    return otherProcessor;
                }
            }
            // TODO adapt this when there are multiple lanes
            return TracksState::getOutputProcessorForTrack(track);
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

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType) {
        for (const auto &track : tracks.getState())
            if (!tracks.isMasterTrack(track))
                for (const auto &processor : TracksState::getProcessorLaneForTrack(track))
                    if (isProcessorAnEffect(processor, connectionType))
                        return true;
        return false;
    }

private:
    ValueTree connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    TracksState &tracks;
};
