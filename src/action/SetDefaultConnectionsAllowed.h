#pragma once

#include "CreateOrDeleteConnections.h"

struct SetDefaultConnectionsAllowed : public CreateOrDeleteConnections {
    SetDefaultConnectionsAllowed(Processor *processor, bool defaultConnectionsAllowed, Connections &connections);

    bool perform() override;
    bool undo() override;

private:
    Processor *processor;
    bool defaultConnectionsAllowed;
};
