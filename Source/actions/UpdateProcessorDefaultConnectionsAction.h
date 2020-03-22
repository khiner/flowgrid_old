#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"

#include <state/Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state/InputState.h>

struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {

    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            ConnectionsState &connections, OutputState &output,
                                            StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connections) {
        for (auto connectionType : {audio, midi}) {
            auto customOutgoingConnections = connections.getConnectionsForNode(processor, connectionType, false, true, true, false);
            if (!customOutgoingConnections.isEmpty())
                continue;
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
