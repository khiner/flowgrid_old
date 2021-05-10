#pragma once

#include "state/ConnectionsState.h"

struct CreateOrDeleteConnectionsAction : public UndoableAction {
    explicit CreateOrDeleteConnectionsAction(ConnectionsState &connections);

    CreateOrDeleteConnectionsAction(CreateOrDeleteConnectionsAction *coalesceLeft, CreateOrDeleteConnectionsAction *coalesceRight,ConnectionsState &connections);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    UndoableAction *createCoalescedAction(UndoableAction *nextAction) override;

    void coalesceWith(const CreateOrDeleteConnectionsAction &other);

    void addConnection(const ValueTree &connection);
    void removeConnection(const ValueTree &connection);

    Array<ValueTree> connectionsToCreate;
    Array<ValueTree> connectionsToDelete;
protected:
    ConnectionsState &connections;
};
