#include "ResetDefaultExternalInputConnectionsAction.h"

#include "DisconnectProcessor.h"
#include "DefaultConnectProcessor.h"

ResetDefaultExternalInputConnectionsAction::ResetDefaultExternalInputConnectionsAction(Connections &connections, Tracks &tracks, Input &input, ProcessorGraph &processorGraph,
                                                                                       ValueTree trackToTreatAsFocused)
        : CreateOrDeleteConnections(connections) {
    if (!trackToTreatAsFocused.isValid())
        trackToTreatAsFocused = tracks.getFocusedTrack();

    for (auto connectionType : {audio, midi}) {
        const auto sourceNodeId = input.getDefaultInputNodeIdForConnectionType(connectionType);

        // If master track received focus, only change the default connections if no other tracks have effect processors
        if (Tracks::isMasterTrack(trackToTreatAsFocused) && connections.anyNonMasterTrackHasEffectProcessor(connectionType)) continue;

        const ValueTree &inputProcessor = processorGraph.getProcessorStateForNodeId(sourceNodeId);
        AudioProcessorGraph::NodeID destinationNodeId;

        const ValueTree &topmostEffectProcessor = findTopmostEffectProcessor(trackToTreatAsFocused, connectionType);
        const auto &destinationProcessor = findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType, tracks, input);
        destinationNodeId = Tracks::getNodeIdForProcessor(destinationProcessor);

        if (destinationNodeId.isValid()) {
            coalesceWith(DefaultConnectProcessor(inputProcessor, destinationNodeId, connectionType, connections, processorGraph));
        }
        coalesceWith(DisconnectProcessor(connections, inputProcessor, connectionType, true, false, false, true, destinationNodeId));
    }
}

ValueTree ResetDefaultExternalInputConnectionsAction::findTopmostEffectProcessor(const ValueTree &track, ConnectionType connectionType) {
    for (const auto &processor : Tracks::getProcessorLaneForTrack(track))
        if (Connections::isProcessorAnEffect(processor, connectionType))
            return processor;
    return {};
}

ValueTree ResetDefaultExternalInputConnectionsAction::findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType, Tracks &tracks, Input &input) {
    if (!processor.isValid())
        return {};

    int lowestSlot = INT_MAX;
    ValueTree upperRightMostProcessor;
    AudioProcessorGraph::NodeID processorNodeId = Tracks::getNodeIdForProcessor(processor);
    if (isAvailableForExternalInput(processor, connectionType, input))
        upperRightMostProcessor = processor;

    // TODO performance improvement: only iterate over connected processors
    for (int i = tracks.getNumTracks() - 1; i >= 0; i--) {
        const auto &lane = Tracks::getProcessorLaneForTrack(tracks.getTrack(i));
        if (lane.getNumChildren() == 0)
            continue;

        const auto &firstProcessor = lane.getChild(0);
        auto firstProcessorNodeId = Tracks::getNodeIdForProcessor(firstProcessor);
        int slot = firstProcessor[ProcessorIDs::processorSlot];
        if (slot < lowestSlot &&
            isAvailableForExternalInput(firstProcessor, connectionType, input) &&
            areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

            lowestSlot = slot;
            upperRightMostProcessor = firstProcessor;
        }
    }

    return upperRightMostProcessor;
}

bool ResetDefaultExternalInputConnectionsAction::isAvailableForExternalInput(const ValueTree &processor, ConnectionType connectionType, Input &input) {
    if (!connections.isProcessorAnEffect(processor, connectionType)) return false;

    const auto &incomingConnections = connections.getConnectionsForNode(processor, connectionType, true, false);
    const auto defaultInputNodeId = input.getDefaultInputNodeIdForConnectionType(connectionType);
    for (const auto &incomingConnection : incomingConnections) {
        if (Tracks::getNodeIdForProcessor(incomingConnection.getChildWithName(ConnectionsIDs::SOURCE)) != defaultInputNodeId)
            return false;
    }
    return true;
}

bool ResetDefaultExternalInputConnectionsAction::areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) {
    if (upstreamNodeId == downstreamNodeId) return true;

    Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
    for (const auto &connection : connections.getState()) {
        if (Tracks::getNodeIdForProcessor(connection.getChildWithName(ConnectionsIDs::SOURCE)) == upstreamNodeId) {
            auto otherDownstreamNodeId = Tracks::getNodeIdForProcessor(connection.getChildWithName(ConnectionsIDs::DESTINATION));
            if (!exploredDownstreamNodes.contains(otherDownstreamNodeId)) {
                if (otherDownstreamNodeId == downstreamNodeId || areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId))
                    return true;
                exploredDownstreamNodes.add(otherDownstreamNodeId);
            }
        }
    }
    return false;
}
