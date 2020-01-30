#pragma once

#include <Identifiers.h>
#include "JuceHeader.h"
#include "CreateOrDeleteConnectionsAction.h"

struct DeleteConnectionAction : public CreateOrDeleteConnectionsAction {
    DeleteConnectionAction(const ValueTree &connection, bool allowCustom, bool allowDefaults,
                           ConnectionsStateManager &connectionsManager)
            : CreateOrDeleteConnectionsAction(connectionsManager) {
        if (canRemoveConnection(connection, allowDefaults, allowCustom))
            removeConnection(connection);
    }

    static bool canRemoveConnection(const ValueTree& connection, bool allowDefaults, bool allowCustom) {
        return (allowCustom && connection.hasProperty(IDs::isCustomConnection)) ||
               (allowDefaults && !connection.hasProperty(IDs::isCustomConnection));
    }

private:
    JUCE_DECLARE_NON_COPYABLE(DeleteConnectionAction)
};
