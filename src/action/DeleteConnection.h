#pragma once

#include "CreateOrDeleteConnections.h"

struct DeleteConnection : public CreateOrDeleteConnections {
    DeleteConnection(const fg::Connection *connection, bool allowCustom, bool allowDefaults, Connections &connections);
};
