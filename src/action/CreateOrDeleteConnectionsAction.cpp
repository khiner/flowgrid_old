#include "CreateOrDeleteConnectionsAction.h"

CreateOrDeleteConnectionsAction::CreateOrDeleteConnectionsAction(Connections &connections)
        : connections(connections) {
}

CreateOrDeleteConnectionsAction::CreateOrDeleteConnectionsAction(CreateOrDeleteConnectionsAction *coalesceLeft, CreateOrDeleteConnectionsAction *coalesceRight, Connections &connections)
        : connections(connections) {
    if (coalesceLeft != nullptr)
        this->coalesceWith(*coalesceLeft);
    if (coalesceRight != nullptr)
        this->coalesceWith(*coalesceRight);
}

bool CreateOrDeleteConnectionsAction::perform() {
    if (connectionsToCreate.isEmpty() && connectionsToDelete.isEmpty()) return false;

    for (const auto &connectionToDelete : connectionsToDelete)
        connections.getState().removeChild(connectionToDelete, nullptr);
    for (const auto &connectionToCreate : connectionsToCreate)
        connections.getState().appendChild(connectionToCreate, nullptr);
    return true;
}

bool CreateOrDeleteConnectionsAction::undo() {
    if (connectionsToCreate.isEmpty() && connectionsToDelete.isEmpty()) return false;

    for (int i = connectionsToCreate.size() - 1; i >= 0; i--)
        connections.getState().removeChild(connectionsToCreate.getUnchecked(i), nullptr);
    for (int i = connectionsToDelete.size() - 1; i >= 0; i--)
        connections.getState().appendChild(connectionsToDelete.getUnchecked(i), nullptr);
    return true;
}

UndoableAction *CreateOrDeleteConnectionsAction::createCoalescedAction(UndoableAction *nextAction) {
    if (auto *nextConnect = dynamic_cast<CreateOrDeleteConnectionsAction *>(nextAction))
        return new CreateOrDeleteConnectionsAction(this, nextConnect, connections);

    return nullptr;
}

void CreateOrDeleteConnectionsAction::coalesceWith(const CreateOrDeleteConnectionsAction &other) {
    for (const auto &connectionToCreate : other.connectionsToCreate)
        addConnection(connectionToCreate);
    for (const auto &connectionToDelete : other.connectionsToDelete)
        removeConnection(connectionToDelete);
}

void CreateOrDeleteConnectionsAction::addConnection(const ValueTree &connection) {
    int deleteIndex = connectionsToDelete.indexOf(connection);
    if (deleteIndex != -1)
        connectionsToDelete.remove(deleteIndex); // cancels out
    else if (!connectionsToCreate.contains(connection))
        connectionsToCreate.add(connection);
}

void CreateOrDeleteConnectionsAction::removeConnection(const ValueTree &connection) {
    int createIndex = connectionsToCreate.indexOf(connection);
    if (createIndex != -1)
        connectionsToCreate.remove(createIndex); // cancels out
    else if (!connectionsToDelete.contains(connection))
        connectionsToDelete.add(connection);
}
