#pragma once

#include "model/Connections.h"

struct CreateOrDeleteConnectionsAction : public UndoableAction {
    explicit CreateOrDeleteConnectionsAction(Connections &connections);

    CreateOrDeleteConnectionsAction(CreateOrDeleteConnectionsAction *coalesceLeft, CreateOrDeleteConnectionsAction *coalesceRight, Connections &connections);

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
    Connections &connections;
};
