#pragma once

#include "CreateOrDeleteConnectionsAction.h"

struct DeleteConnectionAction : public CreateOrDeleteConnectionsAction {
    DeleteConnectionAction(const ValueTree &connection, bool allowCustom, bool allowDefaults, Connections &connections);
};
