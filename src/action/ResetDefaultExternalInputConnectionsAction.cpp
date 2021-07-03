#include "ResetDefaultExternalInputConnectionsAction.h"

#include "DisconnectProcessor.h"
#include "DefaultConnectProcessor.h"

static const Processor *findTopmostEffectProcessor(const Track *track, ConnectionType connectionType) {
    for (const auto *processor : track->getProcessorLane()->getChildren())
        if (processor->isProcessorAnEffect(connectionType))
            return processor;
    return nullptr;
}

ResetDefaultExternalInputConnectionsAction::ResetDefaultExternalInputConnectionsAction(Connections &connections, Tracks &tracks, Input &input, AllProcessors &allProcessors, ProcessorGraph &processorGraph, Track *trackToTreatAsFocused)
        : CreateOrDeleteConnections(connections) {
    if (trackToTreatAsFocused == nullptr)
        trackToTreatAsFocused = tracks.getFocusedTrack();

    for (auto connectionType : {audio, midi}) {
        // If master track received focus, only change the default connections if no other tracks have effect processors
        if (trackToTreatAsFocused->isMaster() && tracks.anyNonMasterTrackHasEffectProcessor(connectionType)) continue;
        const auto *sourceProcessor = input.getDefaultInputProcessorForConnectionType(connectionType);
        if (sourceProcessor == nullptr) continue;

        AudioProcessorGraph::NodeID destinationNodeId;
        const auto *topmostEffectProcessor = findTopmostEffectProcessor(trackToTreatAsFocused, connectionType);
        if (const auto *destinationProcessor = findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType, tracks, input)) {
            destinationNodeId = destinationProcessor->getNodeId();
            coalesceWith(DefaultConnectProcessor(sourceProcessor, destinationNodeId, connectionType, connections, allProcessors, processorGraph));
        }
        coalesceWith(DisconnectProcessor(connections, sourceProcessor, connectionType, true, false, false, true, destinationNodeId));
    }
}

const Processor *ResetDefaultExternalInputConnectionsAction::findMostUpstreamAvailableProcessorConnectedTo(const Processor *processor, ConnectionType connectionType, Tracks &tracks, Input &input) {
    if (processor == nullptr) return {};

    int lowestSlot = INT_MAX;
    const Processor *upperRightMostProcessor = nullptr;
    AudioProcessorGraph::NodeID processorNodeId = processor->getNodeId();
    if (isAvailableForExternalInput(processor, connectionType, input))
        upperRightMostProcessor = processor;

    // TODO performance improvement: only iterate over connected processors
    for (int i = tracks.size() - 1; i >= 0; i--) {
        const auto *track = tracks.get(i);
        const auto *firstProcessor = track->getFirstProcessor();
        if (firstProcessor == nullptr) continue;

        auto firstProcessorNodeId = firstProcessor->getNodeId();
        int slot = firstProcessor->getSlot();
        if (slot < lowestSlot &&
            isAvailableForExternalInput(firstProcessor, connectionType, input) &&
            areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

            lowestSlot = slot;
            upperRightMostProcessor = firstProcessor;
        }
    }

    return upperRightMostProcessor;
}

bool ResetDefaultExternalInputConnectionsAction::isAvailableForExternalInput(const Processor *processor, ConnectionType connectionType, Input &input) {
    if (processor == nullptr || !processor->isProcessorAnEffect(connectionType)) return false;

    const auto incomingConnections = connections.getConnectionsForNode(processor, connectionType, true, false);
    const auto *inputProcessor = input.getDefaultInputProcessorForConnectionType(connectionType);
    if (inputProcessor == nullptr) return false;

    for (const auto *incomingConnection : incomingConnections) {
        if (incomingConnection->getSourceNodeId() != inputProcessor->getNodeId())
            return false;
    }
    return true;
}

bool ResetDefaultExternalInputConnectionsAction::areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) {
    if (upstreamNodeId == downstreamNodeId) return true;

    Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
    for (const auto *connection : connections.getChildren()) {
        if (connection->getSourceNodeId() == upstreamNodeId) {
            auto otherDownstreamNodeId = connection->getDestinationNodeId();
            if (!exploredDownstreamNodes.contains(otherDownstreamNodeId)) {
                if (otherDownstreamNodeId == downstreamNodeId || areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId))
                    return true;
                exploredDownstreamNodes.add(otherDownstreamNodeId);
            }
        }
    }
    return false;
}
