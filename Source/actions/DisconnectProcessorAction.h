#pragma once

#include "actions/DeleteConnectionAction.h"

struct DisconnectProcessorAction : public CreateOrDeleteConnectionsAction {
    DisconnectProcessorAction(ConnectionsState &connections, const ValueTree &processor, ConnectionType connectionType,
                              bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {});
};
