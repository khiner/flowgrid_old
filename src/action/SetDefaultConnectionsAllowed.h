#pragma once

#include "CreateOrDeleteConnections.h"

struct SetDefaultConnectionsAllowed : public CreateOrDeleteConnections {
    SetDefaultConnectionsAllowed(const ValueTree &processor, bool defaultConnectionsAllowed, Connections &connections);

    bool perform() override;
    bool undo() override;

private:
    ValueTree processor;
    bool defaultConnectionsAllowed;
};
