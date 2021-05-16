#include "DeleteConnectionAction.h"

static bool canRemoveConnection(const ValueTree &connection, bool allowDefaults, bool allowCustom) {
    return (allowCustom && connection.hasProperty(ConnectionsStateIDs::isCustomConnection)) ||
           (allowDefaults && !connection.hasProperty(ConnectionsStateIDs::isCustomConnection));
}

DeleteConnectionAction::DeleteConnectionAction(const ValueTree &connection, bool allowCustom, bool allowDefaults, ConnectionsState &connections)
        : CreateOrDeleteConnectionsAction(connections) {
    if (canRemoveConnection(connection, allowDefaults, allowCustom))
        removeConnection(connection);
}
