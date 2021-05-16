#pragma once

#include "action/DeleteConnectionAction.h"

struct DisconnectProcessorAction : public CreateOrDeleteConnectionsAction {
    DisconnectProcessorAction(Connections &connections, const ValueTree &processor, ConnectionType connectionType,
                              bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {});
};
