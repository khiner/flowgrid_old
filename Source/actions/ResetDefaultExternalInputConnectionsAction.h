#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"
#include "DisconnectProcessorAction.h"
#include "DefaultConnectProcessorAction.h"

#include <state/Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state/InputState.h>

// For both audio and midi connection types:
//   * Find the topmost effect processor (receiving audio/midi) in the focused track
//   * Connect external device inputs to its most-upstream connected processor (including itself) that doesn't already have incoming connections
// (Note that it is possible for the same focused track to have a default audio-input processor different
// from its default midi-input processor.)

struct ResetDefaultExternalInputConnectionsAction : public CreateOrDeleteConnectionsAction {

    ResetDefaultExternalInputConnectionsAction(ConnectionsState &connections, TracksState &tracks, InputState &input,
                                               StatefulAudioProcessorContainer &audioProcessorContainer, ValueTree trackToTreatAsFocused = {})
            : CreateOrDeleteConnectionsAction(connections) {
        if (!trackToTreatAsFocused.isValid())
            trackToTreatAsFocused = tracks.getFocusedTrack();

        for (auto connectionType : {audio, midi}) {
            const auto sourceNodeId = input.getDefaultInputNodeIdForConnectionType(connectionType);

            // If master track received focus, only change the default connections if no other tracks have effect processors
            if (TracksState::isMasterTrack(trackToTreatAsFocused) && connections.anyNonMasterTrackHasEffectProcessor(connectionType)) continue;

            const ValueTree &inputProcessor = audioProcessorContainer.getProcessorStateForNodeId(sourceNodeId);
            AudioProcessorGraph::NodeID destinationNodeId;

            const ValueTree &topmostEffectProcessor = findTopmostEffectProcessor(trackToTreatAsFocused, connectionType);
            const auto &destinationProcessor = findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType, tracks, input);
            destinationNodeId = SAPC::getNodeIdForState(destinationProcessor);

            if (destinationNodeId.isValid()) {
                coalesceWith(DefaultConnectProcessorAction(inputProcessor, destinationNodeId, connectionType, connections, audioProcessorContainer));
            }
            coalesceWith(DisconnectProcessorAction(connections, inputProcessor, connectionType, true, false, false, true, destinationNodeId));
        }
    }

private:

    ValueTree findTopmostEffectProcessor(const ValueTree &track, ConnectionType connectionType) {
        for (const auto &processor : track)
            if (connections.isProcessorAnEffect(processor, connectionType))
                return processor;
        return {};
    }

    // Find the upper-right-most effect processor that flows into the given processor
    // which doesn't already have incoming node connections.
    ValueTree findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType,
                                                            TracksState &tracks, InputState &input) {
        if (!processor.isValid())
            return {};

        int lowestSlot = INT_MAX;
        ValueTree upperRightMostProcessor;
        AudioProcessorGraph::NodeID processorNodeId = SAPC::getNodeIdForState(processor);
        if (isAvailableForExternalInput(processor, connectionType, input))
            upperRightMostProcessor = processor;

        // TODO performance improvement: only iterate over connected processors
        for (int i = tracks.getNumTracks() - 1; i >= 0; i--) {
            const auto &track = tracks.getTrack(i);
            if (track.getNumChildren() == 0)
                continue;

            const auto &firstProcessor = track.getChild(0);
            auto firstProcessorNodeId = SAPC::getNodeIdForState(firstProcessor);
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

    bool isAvailableForExternalInput(const ValueTree &processor, ConnectionType connectionType, InputState &input) {
        if (!connections.isProcessorAnEffect(processor, connectionType))
            return false;

        const auto &incomingConnections = connections.getConnectionsForNode(processor, connectionType, true, false);
        const auto defaultInputNodeId = input.getDefaultInputNodeIdForConnectionType(connectionType);
        for (const auto &incomingConnection : incomingConnections)
            if (SAPC::getNodeIdForState(incomingConnection.getChildWithName(IDs::SOURCE)) != defaultInputNodeId)
                return false;

        return true;
    }


    bool areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) {
        if (upstreamNodeId == downstreamNodeId)
            return true;

        Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
        for (const auto &connection : connections.getState()) {
            if (SAPC::getNodeIdForState(connection.getChildWithName(IDs::SOURCE)) == upstreamNodeId) {
                auto otherDownstreamNodeId = SAPC::getNodeIdForState(connection.getChildWithName(IDs::DESTINATION));
                if (!exploredDownstreamNodes.contains(otherDownstreamNodeId)) {
                    if (otherDownstreamNodeId == downstreamNodeId || areProcessorsConnected(otherDownstreamNodeId, downstreamNodeId))
                        return true;
                    exploredDownstreamNodes.add(otherDownstreamNodeId);
                }
            }
        }
        return false;
    }

    JUCE_DECLARE_NON_COPYABLE(ResetDefaultExternalInputConnectionsAction)
};
