#include "ConnectionsState.h"

static bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) {
    return !(!otherProcessor.hasType(IDs::PROCESSOR) || processor == otherProcessor) &&
           ConnectionsState::isProcessorAProducer(processor, connectionType) && ConnectionsState::isProcessorAnEffect(otherProcessor, connectionType);
}

ConnectionsState::ConnectionsState(TracksState &tracks) : tracks(tracks) {
    connections = ValueTree(IDs::CONNECTIONS);
}

ValueTree ConnectionsState::findDefaultDestinationProcessor(const ValueTree &sourceProcessor, ConnectionType connectionType) {
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

bool ConnectionsState::isNodeConnected(AudioProcessorGraph::NodeID nodeId) const {
    for (const auto &connection : connections) {
        if (TracksState::getNodeIdForProcessor(connection.getChildWithName(IDs::SOURCE)) == nodeId)
            return true;
    }
    return false;
}

Array<ValueTree> ConnectionsState::getConnectionsForNode(const ValueTree &processor, ConnectionType connectionType, bool incoming, bool outgoing, bool includeCustom, bool includeDefault) {
    Array<ValueTree> nodeConnections;
    for (const auto &connection : connections) {
        if ((connection[IDs::isCustomConnection] && !includeCustom) || (!connection[IDs::isCustomConnection] && !includeDefault))
            continue;

        int processorNodeId = int(TracksState::getNodeIdForProcessor(processor).uid);
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

bool ConnectionsState::anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType) {
    for (const auto &track : tracks.getState())
        if (!TracksState::isMasterTrack(track))
            for (const auto &processor : TracksState::getProcessorLaneForTrack(track))
                if (isProcessorAnEffect(processor, connectionType))
                    return true;
    return false;
}
