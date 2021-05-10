#pragma once

#include "CreateOrDeleteConnectionsAction.h"
#include "StatefulAudioProcessorContainer.h"

struct DefaultConnectProcessorAction : public CreateOrDeleteConnectionsAction {
    DefaultConnectProcessorAction(const ValueTree &fromProcessor, AudioProcessorGraph::NodeID toNodeId,
                                  ConnectionType connectionType, ConnectionsState &connections,
                                  StatefulAudioProcessorContainer &audioProcessorContainer);
};
