#include "CreateOrDeleteConnections.h"

CreateOrDeleteConnections::CreateOrDeleteConnections(Connections &connections)
        : connections(connections) {
}

CreateOrDeleteConnections::CreateOrDeleteConnections(CreateOrDeleteConnections *coalesceLeft, CreateOrDeleteConnections *coalesceRight, Connections &connections)
        : connections(connections) {
    if (coalesceLeft != nullptr)
        this->coalesceWith(*coalesceLeft);
    if (coalesceRight != nullptr)
        this->coalesceWith(*coalesceRight);
}

bool CreateOrDeleteConnections::perform() {
    if (connectionsToCreate.isEmpty() && connectionsToDelete.isEmpty()) return false;

    for (auto *connectionToDelete : connectionsToDelete)
        connections.removeAudioConnection(connectionToDelete->toAudioConnection());
    for (auto *connectionToCreate : connectionsToCreate)
        connections.append(connectionToCreate);
    return true;
}

bool CreateOrDeleteConnections::undo() {
    if (connectionsToCreate.isEmpty() && connectionsToDelete.isEmpty()) return false;

    for (int i = connectionsToCreate.size() - 1; i >= 0; i--)
        connections.removeAudioConnection(connectionsToCreate.getUnchecked(i)->toAudioConnection());
    for (int i = connectionsToDelete.size() - 1; i >= 0; i--)
        connections.append(connectionsToDelete.getUnchecked(i));
    return true;
}

UndoableAction *CreateOrDeleteConnections::createCoalescedAction(UndoableAction *nextAction) {
    if (auto *nextConnect = dynamic_cast<CreateOrDeleteConnections *>(nextAction))
        return new CreateOrDeleteConnections(this, nextConnect, connections);

    return nullptr;
}

void CreateOrDeleteConnections::coalesceWith(const CreateOrDeleteConnections &other) {
    for (const auto *connectionToCreate : other.connectionsToCreate)
        addConnection(connectionToCreate->toAudioConnection(), !connectionToCreate->isCustom());
    for (const auto *connectionToDelete : other.connectionsToDelete)
        removeConnection(connectionToDelete->toAudioConnection());
}

static int indexOf(OwnedArray<fg::Connection> &connections, const AudioProcessorGraph::Connection &audioConnection) {
    for (int i = 0; i < connections.size(); i++) {
        const auto *connection = connections.getUnchecked(i);
        if (connection->toAudioConnection() == audioConnection) return i;
    }
    return -1;
}

void CreateOrDeleteConnections::addConnection(const AudioProcessorGraph::Connection &audioConnection, bool isDefault) {
    int deleteIndex = indexOf(connectionsToDelete, audioConnection);
    if (deleteIndex != -1) {
        connectionsToDelete.remove(deleteIndex); // cancels out
    } else {
        int createIndex = indexOf(connectionsToCreate, audioConnection);
        if (createIndex == -1) {
            connectionsToCreate.add(std::make_unique<fg::Connection>(audioConnection, isDefault));
        }
    }
}

void CreateOrDeleteConnections::removeConnection(const AudioProcessorGraph::Connection &audioConnection) {
    int createIndex = indexOf(connectionsToCreate, audioConnection);
    if (createIndex != -1) {
        connectionsToCreate.remove(createIndex); // cancels out
    } else {
        int deleteIndex = indexOf(connectionsToDelete, audioConnection);
        if (deleteIndex == -1) {
            connectionsToDelete.add(std::make_unique<fg::Connection>(audioConnection));
        }
    }

}
