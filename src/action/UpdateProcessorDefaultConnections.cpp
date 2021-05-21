#include "UpdateProcessorDefaultConnections.h"

#include "DefaultConnectProcessor.h"
#include "DisconnectProcessor.h"

UpdateProcessorDefaultConnections::UpdateProcessorDefaultConnections(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                                                     Connections &connections, Output &output, ProcessorGraph &processorGraph)
        : CreateOrDeleteConnections(connections) {
    for (auto connectionType : {audio, midi}) {
        auto customOutgoingConnections = connections.getConnectionsForNode(processor, connectionType, false, true, true, false);
        if (!customOutgoingConnections.isEmpty()) continue;

        auto processorToConnectTo = connections.findDefaultDestinationProcessor(processor, connectionType);
        if (!processorToConnectTo.isValid())
            processorToConnectTo = output.getAudioOutputProcessorState();
        auto nodeIdToConnectTo = Processor::getNodeId(processorToConnectTo);

        auto disconnectDefaultsAction = DisconnectProcessor(connections, processor, connectionType, true, false, false, true, nodeIdToConnectTo);
        coalesceWith(disconnectDefaultsAction);
        if (makeInvalidDefaultsIntoCustom) {
            if (!disconnectDefaultsAction.connectionsToDelete.isEmpty()) {
                for (const auto &connectionToConvert : disconnectDefaultsAction.connectionsToDelete) {
                    auto customConnection = connectionToConvert.createCopy();
                    customConnection.setProperty(ConnectionIDs::isCustomConnection, true, nullptr);
                    connectionsToCreate.add(customConnection);
                }
            }
        } else {
            coalesceWith(DefaultConnectProcessor(processor, nodeIdToConnectTo, connectionType, connections, processorGraph));
        }
    }
}
