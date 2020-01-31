#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"
#include "DisconnectProcessorAction.h"
#include "DefaultConnectProcessorAction.h"

#include <Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state/InputStateManager.h>

// Disconnect external audio/midi inputs (unless `addDefaultConnections` is true and
// the default connection would stay the same).
// If `addDefaultConnections` is true, then for both audio and midi connection types:
//   * Find the topmost effect processor (receiving audio/midi) in the focused track
//   * Connect external device inputs to its most-upstream connected processor (including itself)
// (Note that it is possible for the same focused track to have a default audio-input processor different
// from its default midi-input processor.)

// TODO should receive a processor rather than using state to find the current focused processor
// That way, we can deterministically find all the added/removed connections resulting from
// changing default connections before and after a focus change, without needing to actually
// change the focus state temporarily
struct ResetDefaultExternalInputConnectionsAction : public CreateOrDeleteConnectionsAction {

    ResetDefaultExternalInputConnectionsAction(bool addDefaultConnections, ConnectionsStateManager& connectionsManager,
                                               TracksStateManager& tracksManager, InputStateManager& inputManager,
                                               StatefulAudioProcessorContainer &audioProcessorContainer,
                                               ValueTree trackToTreatAsFocused={})
            : CreateOrDeleteConnectionsAction(connectionsManager) {
        if (!trackToTreatAsFocused.isValid())
            trackToTreatAsFocused = tracksManager.getFocusedTrack();

        const auto audioSourceNodeId = inputManager.getDefaultInputNodeIdForConnectionType(audio);
        const auto midiSourceNodeId = inputManager.getDefaultInputNodeIdForConnectionType(midi);
        const ValueTree &audioInputProcessor = audioProcessorContainer.getProcessorStateForNodeId(audioSourceNodeId);
        const ValueTree &midiInputProcessor = audioProcessorContainer.getProcessorStateForNodeId(midiSourceNodeId);

        AudioProcessorGraph::NodeID audioDestinationNodeId, midiDestinationNodeId;
        if (addDefaultConnections) {
            audioDestinationNodeId = SAPC::getNodeIdForState(findEffectProcessorToReceiveDefaultExternalInput(audio, tracksManager, inputManager, trackToTreatAsFocused));
            midiDestinationNodeId = SAPC::getNodeIdForState(findEffectProcessorToReceiveDefaultExternalInput(midi, tracksManager, inputManager, trackToTreatAsFocused));
        }

        coalesceWith(DefaultConnectProcessorAction( audioInputProcessor, audioDestinationNodeId, audio, connectionsManager, audioProcessorContainer));
        coalesceWith(DisconnectProcessorAction(connectionsManager, audioInputProcessor, audio, true, false, false, true, audioDestinationNodeId));

        coalesceWith(DefaultConnectProcessorAction(midiInputProcessor, midiDestinationNodeId, midi, connectionsManager, audioProcessorContainer));
        coalesceWith(DisconnectProcessorAction(connectionsManager, midiInputProcessor, midi, true, false, false, true, midiDestinationNodeId));
    }

private:

    ValueTree findEffectProcessorToReceiveDefaultExternalInput(ConnectionType connectionType, TracksStateManager& tracksManager, InputStateManager& inputManager, const ValueTree& focusedTrack) {
        const ValueTree &topmostEffectProcessor = findTopmostEffectProcessor(focusedTrack, connectionType);
        return findMostUpstreamAvailableProcessorConnectedTo(topmostEffectProcessor, connectionType, tracksManager, inputManager);
    }

    ValueTree findTopmostEffectProcessor(const ValueTree& track, ConnectionType connectionType) {
        for (const auto& processor : track) {
            if (connectionsManager.isProcessorAnEffect(processor, connectionType)) {
                return processor;
            }
        }
        return {};
    }

    // Find the upper-right-most processor that flows into the given processor
    // which doesn't already have incoming node connections.
    ValueTree findMostUpstreamAvailableProcessorConnectedTo(const ValueTree &processor, ConnectionType connectionType,
                                                            TracksStateManager& tracksManager, InputStateManager& inputManager) {
        if (!processor.isValid())
            return {};

        int lowestSlot = INT_MAX;
        ValueTree upperRightMostProcessor;
        AudioProcessorGraph::NodeID processorNodeId = SAPC::getNodeIdForState(processor);
        if (isAvailableForExternalInput(processor, connectionType, inputManager))
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
                isAvailableForExternalInput(firstProcessor, connectionType, inputManager) &&
                areProcessorsConnected(firstProcessorNodeId, processorNodeId)) {

                lowestSlot = slot;
                upperRightMostProcessor = firstProcessor;
            }
        }

        return upperRightMostProcessor;
    }

    bool isAvailableForExternalInput(const ValueTree& processor, ConnectionType connectionType, InputStateManager& inputManager) {
        const auto& incomingConnections = getConnectionsForNode(processor, connectionType, true, false);
        const auto defaultInputNodeId = inputManager.getDefaultInputNodeIdForConnectionType(connectionType);
        for (const auto& incomingConnection : incomingConnections) {
            if (SAPC::getNodeIdForState(incomingConnection.getChildWithName(IDs::SOURCE)) != defaultInputNodeId) {
                return false;
            }
        }
        return true;
    }


    bool areProcessorsConnected(AudioProcessorGraph::NodeID upstreamNodeId, AudioProcessorGraph::NodeID downstreamNodeId) {
        if (upstreamNodeId == downstreamNodeId)
            return true;

        Array<AudioProcessorGraph::NodeID> exploredDownstreamNodes;
        for (const auto& connection : connectionsManager.getState()) {
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
