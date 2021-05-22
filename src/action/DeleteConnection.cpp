#include "DeleteConnection.h"

static bool canRemoveConnection(const ValueTree &connection, bool allowDefaults, bool allowCustom) {
    return (allowCustom && fg::Connection::isCustom(connection)) || (allowDefaults && !fg::Connection::isCustom(connection));
}

DeleteConnection::DeleteConnection(const ValueTree &connection, bool allowCustom, bool allowDefaults, Connections &connections)
        : CreateOrDeleteConnections(connections) {
    if (canRemoveConnection(connection, allowDefaults, allowCustom))
        removeConnection(connection);
}
