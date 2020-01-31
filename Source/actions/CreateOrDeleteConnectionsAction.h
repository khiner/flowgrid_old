#pragma once

#include <state/Identifiers.h>

#include <utility>
#include <state/ConnectionsState.h>
#include "JuceHeader.h"

struct CreateOrDeleteConnectionsAction : public UndoableAction {
    explicit CreateOrDeleteConnectionsAction(ConnectionsState& connections)
            : connections(connections) {
    }

    bool perform() override {
        if (connectionsToCreate.isEmpty() && connectionsToDelete.isEmpty())
            return false;

        for (const auto& connectionToCreate : connectionsToCreate)
            connections.getState().appendChild(connectionToCreate, nullptr);
        for (const auto& connectionToDelete : connectionsToDelete)
            connections.getState().removeChild(connectionToDelete, nullptr);
        return true;
    }

    bool undo() override {
        if (connectionsToCreate.isEmpty() && connectionsToDelete.isEmpty())
            return false;

        for (const auto& connectionToDelete : connectionsToDelete)
            connections.getState().appendChild(connectionToDelete, nullptr);
        for (const auto& connectionToCreate : connectionsToCreate)
            connections.getState().removeChild(connectionToCreate, nullptr);
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

    UndoableAction* createCoalescedAction(UndoableAction* nextAction) override {
        if (auto* nextConnect = dynamic_cast<CreateOrDeleteConnectionsAction*>(nextAction)) {
            auto *coalesced = new CreateOrDeleteConnectionsAction(connections);
            coalesced->coalesceWith(*this);
            coalesced->coalesceWith(*nextConnect);
            return coalesced;
        }

        return nullptr;
    }

    void coalesceWith(const CreateOrDeleteConnectionsAction& other) {
        for (const auto& connectionToCreate : other.connectionsToCreate)
            addConnection(connectionToCreate);
        for (const auto& connectionToDelete : other.connectionsToDelete)
            removeConnection(connectionToDelete);
    }

    Array<ValueTree> connectionsToCreate;
    Array<ValueTree> connectionsToDelete;

protected:
    ConnectionsState &connections;

    void addConnection(const ValueTree& connection) {
        int deleteIndex = connectionsToDelete.indexOf(connection);
        if (deleteIndex != -1) {
            connectionsToDelete.remove(deleteIndex); // cancels out
        } else if (!connectionsToCreate.contains(connection)) {
            connectionsToCreate.add(connection);
        }
    }

    void removeConnection(const ValueTree& connection) {
        int createIndex = connectionsToCreate.indexOf(connection);
        if (createIndex != -1) {
            connectionsToCreate.remove(createIndex); // cancels out
        } else if (!connectionsToDelete.contains(connection)) {
            connectionsToDelete.add(connection);
        }
    }

    Array<ValueTree> getConnectionsForNode(const ValueTree& processor, ConnectionType connectionType,
                                           bool incoming=true, bool outgoing=true,
                                           bool includeCustom=true, bool includeDefault=true) {
        Array<ValueTree> nodeConnections;
        for (const auto& connection : connections.getState()) {
            if ((connection[IDs::isCustomConnection] && !includeCustom) || (!connection[IDs::isCustomConnection] && !includeDefault))
                continue;

            int processorNodeId = int(StatefulAudioProcessorContainer::getNodeIdForState(processor).uid);
            const auto &endpointType = connection.getChildWithProperty(IDs::nodeId, processorNodeId);
            bool directionIsAcceptable = (incoming && endpointType.hasType(IDs::DESTINATION)) || (outgoing && endpointType.hasType(IDs::SOURCE));
            bool typeIsAcceptable = connectionType == all ||
                                    (connectionType == audio && int(endpointType[IDs::channel]) != AudioProcessorGraph::midiChannelIndex) ||
                                    (connectionType == midi && int(endpointType[IDs::channel]) == AudioProcessorGraph::midiChannelIndex);

            if (directionIsAcceptable && typeIsAcceptable)
                nodeConnections.add(connection);
        }

        return nodeConnections;
    }
private:

    JUCE_DECLARE_NON_COPYABLE(CreateOrDeleteConnectionsAction)
};
