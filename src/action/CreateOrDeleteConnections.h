#pragma once

#include "model/Connections.h"

struct CreateOrDeleteConnections : public UndoableAction {
    explicit CreateOrDeleteConnections(Connections &connections);

    CreateOrDeleteConnections(CreateOrDeleteConnections *coalesceLeft, CreateOrDeleteConnections *coalesceRight, Connections &connections);

    bool perform() override;

    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

    UndoableAction *createCoalescedAction(UndoableAction *nextAction) override;

    void coalesceWith(const CreateOrDeleteConnections &other);

    void addConnection(const ValueTree &connection);

    void removeConnection(const ValueTree &connection);

    Array<ValueTree> connectionsToCreate;
    Array<ValueTree> connectionsToDelete;
protected:
    Connections &connections;
};
