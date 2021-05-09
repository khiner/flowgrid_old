#pragma once

#include <state/Identifiers.h>
#include "CreateOrDeleteConnectionsAction.h"

struct DeleteConnectionAction : public CreateOrDeleteConnectionsAction {
    DeleteConnectionAction(const ValueTree &connection, bool allowCustom, bool allowDefaults, ConnectionsState &connections)
            : CreateOrDeleteConnectionsAction(connections) {
        if (canRemoveConnection(connection, allowDefaults, allowCustom))
            removeConnection(connection);
    }

    static bool canRemoveConnection(const ValueTree &connection, bool allowDefaults, bool allowCustom) {
        return (allowCustom && connection.hasProperty(IDs::isCustomConnection)) ||
               (allowDefaults && !connection.hasProperty(IDs::isCustomConnection));
    }
};
