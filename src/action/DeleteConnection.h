#pragma once

#include "CreateOrDeleteConnections.h"

struct DeleteConnection : public CreateOrDeleteConnections {
    DeleteConnection(const ValueTree &connection, bool allowCustom, bool allowDefaults, Connections &connections);
};
