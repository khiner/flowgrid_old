#include "ResetDefaultExternalInputConnectionsAction.h"

#include "state/Identifiers.h"
#include "DisconnectProcessorAction.h"
#include "DefaultConnectProcessorAction.h"

ResetDefaultExternalInputConnectionsAction::ResetDefaultExternalInputConnectionsAction(ConnectionsState &connections, TracksState &tracks, InputState &input, ProcessorGraph &processorGraph,
                                                                                       ValueTree trackToTreatAsFocused)
        : CreateOrDeleteConnectionsAction(connections) {
    if (!trackToTreatAsFocused.isValid())
        trackToTreatAsFocused = tracks.getFocusedTrack();

    for (auto connectionType : {audio, midi}) {
        const auto sourceNodeId = input.getDefaultInputNodeIdForConnectionType(connectionType);

        // If master track received focus, only change the default connections if no other tracks have effect processors
        if (TracksState::isMasterTrack(trackToTreatAsFocused) && connections.anyNonMasterTrackHasEffectProcessor(connectionType)) continue;

        const ValueTree &inputProcessor = processorGraph.getProcessorStateForNodeId(sourceNodeId);
        AudioProcessorGraph::NodeID destinationNodeId;

        const ValueTree &topmostEffectProcessor = findTopmostEffectProcessor(trackToTreatAsFocused, connectionType);
        const auto &destinationProcessor = findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType, tracks, input);
        destinationNodeId = TracksState::getNodeIdForProcessor(destinationProcessor);

        if (destinationNodeId.isValid()) {
            coalesceWith(DefaultConnectProcessorAction(inputProcessor, destinationNodeId, connectionType, connections, processorGraph));
        }
        coalesceWith(DisconnectProcessorAction(connections, inputProcessor, connectionType, true, false, false, true, destinationNodeId));
    }
}

ValueTree ResetDefaultExternalInputConnectionsAction::findTopmostEffectProcessor(const ValueTree &track, ConnectionType connectionType) {
    for (const auto &processor : TracksState::getProcessorLaneForTrack(track))
        if (ConnectionsState::isProcessorAnEffect(processor, connectionType))
            return processor;
    return {};
}

ValueTree ResetDefaultExternalInputConnectionsAction::findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType, TracksState &tracks, InputState &input) {
    if (!processor.isValid())
        return {};

    int lowestSlot = INT_MAX;
    ValueTree upperRightMostProcessor;
    AudioProcessorGraph::NodeID processorNodeId = TracksState::getNodeIdForProcessor(processor);
    if (isAvailableForExternalInput(processor, connectionType, input))
        upperRightMostProcessor = processor;

    // TODO performance improvement: only iterate over connected processors
    for (int i = tracks.getNumTracks() - 1; i >= 0; i--) {
        const auto &lane = TracksState::getProcessorLaneForTrack(tracks.getTrack(i));
        if (lane.getNumChildren() == 0)
            continue;

        const auto &firstProcessor = lane.getChild(0);
        auto firstProcessorNodeId = TracksState::getNodeIdForProcessor(firstProcessor);
        int slot = firstProcessor[IDs::processorSlot];
        if (slot < lowestSlot &&
            isAvailableForExternalInput(firstProcessor, connectionType, input) &&
            areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

            lowestSlot = slot;
            upperRightMostProcessor = firstProcessor;
        }
    }

    return upperRightMostProcessor;
}

bool ResetDefaultExternalInputConnectionsAction::isAvailableForExternalInput(const ValueTree &processor, ConnectionType connectionType, InputState &input) {
    if (!connections.isProcessorAnEffect(processor, connectionType)) return false;

    const auto &incomingConnections = connections.getConnectionsForNode(processor, connectionType, true, false);
    const auto defaultInputNodeId = input.getDefaultInputNodeIdForConnectionType(connectionType);
    for (const auto &incomingConnection : incomingConnections) {
        if (TracksState::getNodeIdForProcessor(incomingConnection.getChildWithName(ConnectionsStateIDs::SOURCE)) != defaultInputNodeId)
            return false;
    }
    return true;
}

bool ResetDefaultExternalInputConnectionsAction::areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) {
    if (upstreamNodeId == downstreamNodeId) return true;

    Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
    for (const auto &connection : connections.getState()) {
        if (TracksState::getNodeIdForProcessor(connection.getChildWithName(ConnectionsStateIDs::SOURCE)) == upstreamNodeId) {
            auto otherDownstreamNodeId = TracksState::getNodeIdForProcessor(connection.getChildWithName(ConnectionsStateIDs::DESTINATION));
            if (!exploredDownstreamNodes.contains(otherDownstreamNodeId)) {
                if (otherDownstreamNodeId == downstreamNodeId || areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId))
                    return true;
                exploredDownstreamNodes.add(otherDownstreamNodeId);
            }
        }
    }
    return false;
}
