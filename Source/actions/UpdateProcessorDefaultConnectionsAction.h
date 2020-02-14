#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"

#include <state/Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state/InputState.h>

// Disconnect external audio/midi inputs (unless `addDefaultConnections` is true and
// the default connection would stay the same).
// If `addDefaultConnections` is true, then for both audio and midi connection types:
//   * Find the topmost effect processor (receiving audio/midi) in the focused track
//   * Connect external device inputs to its most-upstream connected processor (including itself)
// (Note that it is possible for the same focused track to have a default audio-input processor different
// from its default midi-input processor.)
struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {

    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            ConnectionsState &connections, OutputState &output,
                                            StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connections) {
        for (auto connectionType : {audio, midi}) {
            auto processorToConnectTo = connections.findProcessorToFlowInto(processor.getParent(), processor, connectionType);
            if (!processorToConnectTo.isValid())
                processorToConnectTo = output.getAudioOutputProcessorState();
            auto nodeIdToConnectTo = StatefulAudioProcessorContainer::getNodeIdForState(processorToConnectTo);

            auto disconnectDefaultsAction = DisconnectProcessorAction(connections, processor, connectionType, true, false, false, true, nodeIdToConnectTo);
            coalesceWith(disconnectDefaultsAction);
            if (makeInvalidDefaultsIntoCustom && !disconnectDefaultsAction.connectionsToDelete.isEmpty()) {
                for (const auto &connectionToConvert : disconnectDefaultsAction.connectionsToDelete) {
                    auto customConnection = connectionToConvert.createCopy();
                    customConnection.setProperty(IDs::isCustomConnection, true, nullptr);
                    connectionsToCreate.add(customConnection);
                }
            } else {
                coalesceWith(DefaultConnectProcessorAction(processor, nodeIdToConnectTo, connectionType, connections, audioProcessorContainer));
            }
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE(UpdateProcessorDefaultConnectionsAction)
};
