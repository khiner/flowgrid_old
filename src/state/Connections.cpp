#include "Connections.h"

static bool canProcessorDefaultConnectTo(const ValueTree &processor, const ValueTree &otherProcessor, ConnectionType connectionType) {
    return !(!otherProcessor.hasType(ProcessorIDs::PROCESSOR) || processor == otherProcessor) &&
           Connections::isProcessorAProducer(processor, connectionType) && Connections::isProcessorAnEffect(otherProcessor, connectionType);
}

ValueTree Connections::findDefaultDestinationProcessor(const ValueTree &sourceProcessor, ConnectionType connectionType) {
    if (!isProcessorAProducer(sourceProcessor, connectionType))
        return {};

    const ValueTree &track = Tracks::getTrackForProcessor(sourceProcessor);
    const auto &masterTrack = tracks.getMasterTrack();
    if (Tracks::isTrackOutputProcessor(sourceProcessor)) {
        if (track == masterTrack)
            return {};
        if (masterTrack.isValid())
            return Tracks::getInputProcessorForTrack(masterTrack);
        return {};
    }

    bool isTrackInputProcessor = Tracks::isTrackInputProcessor(sourceProcessor);
    const auto &lane = Tracks::getProcessorLaneForProcessor(sourceProcessor);
    int siblingDelta = 0;
    ValueTree otherLane;
    while ((otherLane = lane.getSibling(siblingDelta++)).isValid()) {
        for (const auto &otherProcessor : otherLane) {
            if (otherProcessor == sourceProcessor) continue;
            bool isOtherProcessorBelow = isTrackInputProcessor || int(otherProcessor[ProcessorIDs::processorSlot]) > int(sourceProcessor[ProcessorIDs::processorSlot]);
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
        return Tracks::getOutputProcessorForTrack(track);
    }

    return {};
}

bool Connections::isNodeConnected(AudioProcessorGraph::NodeID nodeId) const {
    for (const auto &connection : state) {
        if (Tracks::getNodeIdForProcessor(connection.getChildWithName(ConnectionsIDs::SOURCE)) == nodeId)
            return true;
    }
    return false;
}

Array<ValueTree> Connections::getConnectionsForNode(const ValueTree &processor, ConnectionType connectionType, bool incoming, bool outgoing, bool includeCustom, bool includeDefault) {
    Array<ValueTree> nodeConnections;
    for (const auto &connection : state) {
        if ((connection[ConnectionsIDs::isCustomConnection] && !includeCustom) || (!connection[ConnectionsIDs::isCustomConnection] && !includeDefault))
            continue;

        int processorNodeId = int(Tracks::getNodeIdForProcessor(processor).uid);
        const auto &endpointType = connection.getChildWithProperty(ProcessorIDs::nodeId, processorNodeId);
        bool directionIsAcceptable = (incoming && endpointType.hasType(ConnectionsIDs::DESTINATION)) || (outgoing && endpointType.hasType(ConnectionsIDs::SOURCE));
        bool typeIsAcceptable = connectionType == all ||
                                (connectionType == audio && int(endpointType[ConnectionsIDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                (connectionType == midi && int(endpointType[ConnectionsIDs::channel]) == AudioProcessorGraph::midiChannelIndex);

        if (directionIsAcceptable && typeIsAcceptable)
            nodeConnections.add(connection);
    }

    return nodeConnections;
}

bool Connections::anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType) {
    for (const auto &track : tracks.getState())
        if (!Tracks::isMasterTrack(track))
            for (const auto &processor : Tracks::getProcessorLaneForTrack(track))
                if (isProcessorAnEffect(processor, connectionType))
                    return true;
    return false;
}
