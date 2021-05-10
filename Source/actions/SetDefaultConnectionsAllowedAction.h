#pragma once

#include "CreateOrDeleteConnectionsAction.h"

struct SetDefaultConnectionsAllowedAction : public CreateOrDeleteConnectionsAction {
    SetDefaultConnectionsAllowedAction(const ValueTree &processor, bool defaultConnectionsAllowed, ConnectionsState &connections);

    bool perform() override;
    bool undo() override;

private:
    ValueTree processor;
    bool defaultConnectionsAllowed;
};
