#pragma once

#include "state/Identifiers.h"
#include "state/InputState.h"
#include "state/OutputState.h"
#include "StatefulAudioProcessorContainer.h"
#include "CreateConnectionAction.h"

struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {

    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            ConnectionsState &connections, OutputState &output,
                                            StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connections) {
        for (auto connectionType : {audio, midi}) {
            auto customOutgoingConnections = connections.getConnectionsForNode(processor, connectionType, false, true, true, false);
            if (!customOutgoingConnections.isEmpty())
                continue;
            auto processorToConnectTo = connections.findDefaultDestinationProcessor(processor, connectionType);
            if (!processorToConnectTo.isValid())
                processorToConnectTo = output.getAudioOutputProcessorState();
            auto nodeIdToConnectTo = StatefulAudioProcessorContainer::getNodeIdForState(processorToConnectTo);

            auto disconnectDefaultsAction = DisconnectProcessorAction(connections, processor, connectionType, true, false, false, true, nodeIdToConnectTo);
            coalesceWith(disconnectDefaultsAction);
            if (makeInvalidDefaultsIntoCustom) {
                if (!disconnectDefaultsAction.connectionsToDelete.isEmpty()) {
                    for (const auto &connectionToConvert : disconnectDefaultsAction.connectionsToDelete) {
                        auto customConnection = connectionToConvert.createCopy();
                        customConnection.setProperty(IDs::isCustomConnection, true, nullptr);
                        connectionsToCreate.add(customConnection);
                    }
                }
            } else {
                coalesceWith(DefaultConnectProcessorAction(processor, nodeIdToConnectTo, connectionType, connections, audioProcessorContainer));
            }
        }
    }
};
