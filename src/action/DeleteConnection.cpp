#include "DeleteConnection.h"

static bool canRemoveConnection(const fg::Connection *connection, bool allowDefaults, bool allowCustom) {
    return (allowCustom && connection->isCustom()) || (allowDefaults && !connection->isCustom());
}

DeleteConnection::DeleteConnection(const fg::Connection *connection, bool allowCustom, bool allowDefaults, Connections &connections)
        : CreateOrDeleteConnections(connections) {
    if (canRemoveConnection(connection, allowDefaults, allowCustom))
        removeConnection(connection->toAudioConnection());
}
