#pragma once

#include "action/DeleteConnection.h"

struct DisconnectProcessor : public CreateOrDeleteConnections {
    DisconnectProcessor(Connections &connections, const ValueTree &processor, ConnectionType connectionType,
                        bool defaults, bool custom, bool incoming, bool outgoing, AudioProcessorGraph::NodeID excludingRemovalTo = {});
};
