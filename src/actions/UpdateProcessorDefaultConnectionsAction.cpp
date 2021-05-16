#include "UpdateProcessorDefaultConnectionsAction.h"

#include "DefaultConnectProcessorAction.h"
#include "DisconnectProcessorAction.h"

UpdateProcessorDefaultConnectionsAction::UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                                                                 ConnectionsState &connections, OutputState &output, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnectionsAction(connections) {
    for (auto connectionType : {audio, midi}) {
        auto customOutgoingConnections = connections.getConnectionsForNode(processor, connectionType, false, true, true, false);
        if (!customOutgoingConnections.isEmpty()) continue;

        auto processorToConnectTo = connections.findDefaultDestinationProcessor(processor, connectionType);
        if (!processorToConnectTo.isValid())
            processorToConnectTo = output.getAudioOutputProcessorState();
        auto nodeIdToConnectTo = TracksState::getNodeIdForProcessor(processorToConnectTo);

        auto disconnectDefaultsAction = DisconnectProcessorAction(connections, processor, connectionType, true, false, false, true, nodeIdToConnectTo);
        coalesceWith(disconnectDefaultsAction);
        if (makeInvalidDefaultsIntoCustom) {
            if (!disconnectDefaultsAction.connectionsToDelete.isEmpty()) {
                for (const auto &connectionToConvert : disconnectDefaultsAction.connectionsToDelete) {
                    auto customConnection = connectionToConvert.createCopy();
                    customConnection.setProperty(ConnectionsStateIDs::isCustomConnection, true, nullptr);
                    connectionsToCreate.add(customConnection);
                }
            }
        } else {
            coalesceWith(DefaultConnectProcessorAction(processor, nodeIdToConnectTo, connectionType, connections, processorGraph));
        }
    }
}
