#pragma once

#include "CreateOrDeleteConnections.h"

struct SetDefaultConnectionsAllowed : public CreateOrDeleteConnections {
    SetDefaultConnectionsAllowed(Processor *, bool defaultConnectionsAllowed, Connections &);

    bool perform() override;
    bool undo() override;

private:
    Processor *processor;
    bool defaultConnectionsAllowed;
};
