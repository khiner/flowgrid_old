#include "DeleteConnection.h"

static bool canRemoveConnection(const ValueTree &connection, bool allowDefaults, bool allowCustom) {
    return (allowCustom && connection.hasProperty(ConnectionIDs::isCustomConnection)) ||
           (allowDefaults && !connection.hasProperty(ConnectionIDs::isCustomConnection));
}

DeleteConnection::DeleteConnection(const ValueTree &connection, bool allowCustom, bool allowDefaults, Connections &connections)
        : CreateOrDeleteConnections(connections) {
    if (canRemoveConnection(connection, allowDefaults, allowCustom))
        removeConnection(connection);
}
