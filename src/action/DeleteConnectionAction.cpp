#include "DeleteConnectionAction.h"

static bool canRemoveConnection(const ValueTree &connection, bool allowDefaults, bool allowCustom) {
    return (allowCustom && connection.hasProperty(ConnectionsIDs::isCustomConnection)) ||
           (allowDefaults && !connection.hasProperty(ConnectionsIDs::isCustomConnection));
}

DeleteConnectionAction::DeleteConnectionAction(const ValueTree &connection, bool allowCustom, bool allowDefaults, Connections &connections)
        : CreateOrDeleteConnectionsAction(connections) {
    if (canRemoveConnection(connection, allowDefaults, allowCustom))
        removeConnection(connection);
}
